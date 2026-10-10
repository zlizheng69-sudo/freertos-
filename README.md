# STM32F103 + FreeRTOS 串口命令控制固件

个人学习项目：在 STM32F103C8T6 上用 FreeRTOS（CMSIS-RTOS2 封装）实现一个
**多任务 + 不定长串口帧解析**的命令控制固件。

## 硬件与工具链

| 项 | 值 |
|---|---|
| MCU | STM32F103C8T6（Cortex-M3，72MHz，64KB Flash / 20KB RAM） |
| RTOS | FreeRTOS + CMSIS-RTOS2 |
| 外设 | USART1(PA9/PA10) + DMA1_Channel5 + TIM4(时基) |
| 工具链 | VS Code + STM32CubeMX + CMake + Ninja + arm-none-eabi-gcc |
| 调试 | OpenOCD + GDB |

## 架构与数据流

```
上位机/串口 ──► USART1 ──► DMA1_Ch5 (rx_dma_buf[128])
                              │ 空闲线检测(IDLE) 或 DMA 缓冲满触发
                              ▼
                 HAL_UARTEx_RxEventCallback(huart, Size)   [中断上下文]
                    ① ring_write()        只拷贝 + 计数
                    ② osSemaphoreRelease() 只发信号
                    ③ 重新武装接收 + 关半传输(HT)中断
                              ▼
                 ring buffer s_buf[512]  ── 单生产者/单消费者，无锁
                              ▼
                 CommandTask (osPriorityHigh1)             [任务上下文]
                    frame_parser_feed(byte)  四状态机
                    ├─ FP_FRAME   → 执行命令 / LEN==0 回传统计
                    ├─ FP_CRC_ERR → 计数
                    └─ FP_LEN_ERR → 计数 + 重同步
                              ▼
                          LEDQueue ──► LED 驱动任务 (osPriorityHigh)
```

> **关于任务命名**：`freertos.c` 的线程注册存在函数名与文件名的交叉 ——
> 定义在 `LEDTask.c` 的 `StartKeyTask()`（实际消费 LEDQueue）被注册为 `KeyTask`（High），
> 定义在 `KeyTask.c` 的 `startLEDTask()`（实际做按键扫描）被注册为 `LEDTask`（Normal）。
> 上图按**实际职责**绘制。这是已知技术债，见下方「已知待改进」。

## 通信协议

| 偏移 | 字段 | 说明 |
|---|---|---|
| 0 | `0xAA` | 帧头 1 |
| 1 | `0x55` | 帧头 2 |
| 2 | `LEN` | DATA 长度，0~250；**LEN=0 约定为查询统计** |
| 3 | `SEQ` | 序号 |
| 4 .. 4+LEN-1 | `DATA` | 载荷（LED 命令为 颜色/状态 字节对） |
| 4+LEN .. 5+LEN | `CRC16` | CRC-16/MODBUS，**小端（低字节在前）** |

- 整帧长度 = `LEN + 6`，最大 256 字节
- CRC 覆盖范围：`LEN + SEQ + DATA`
- CRC 参数：初值 `0xFFFF`，多项式 `0xA001`（反射），无最终异或

### STAT 回传格式

```
STAT ok=<n> crc=<n> len=<n> ovr=<n> resync=<n> err=<n> seq=<n>
```

| 字段 | 含义 |
|---|---|
| `ok` | CRC 通过并已执行的帧数 |
| `crc` | CRC 校验失败帧数 |
| `len` | LEN 字段非法次数 |
| `ovr` | 环形缓冲满而丢弃的字节数 |
| `resync` | 帧间超时（20ms 半帧）导致的重同步次数 |
| `err` | UART 错误回调次数（ORE/FE/NE/PE） |
| `seq` | 最近一帧的 SEQ，用于确认链路是否在推进 |

## 关键设计

### 1. 无锁环形缓冲（`usart_ring.c`）
`s_head` 只被中断里的写函数修改，`s_tail` 只被任务里的读函数修改 ——
**每个变量只有单个写者**，加上 Cortex-M3 上对齐的 16 位访问是单条原子指令，
再配合 `volatile` 防止编译器把值缓存进寄存器，因此**全程不需要临界区**。
（代价：一旦加入第二个生产者，这个前提就失效了。）

环形缓冲满时**截断而不覆盖未读数据**，把丢弃字节数计入 `overrun`。
容量取 2 的幂（512），用掩码取模处理回绕，跨界时分两段 `memcpy`。

### 2. 四状态帧解析机（`frame_parser.c`）
`S_HEAD1 → S_HEAD2 → S_LEN → S_BODY`，逐字节喂入，一次返回一个结果：
- **粘包**：一帧收完立即回到 `S_HEAD1`，天然拆分
- **`AA AA 55`**：不丢弃当前字节，把它当作新的帧头候选，避免漏帧
- **LEN 越界**：收 body 之前就校验，缓冲不会被写爆
- **半帧卡死**：任务侧 20ms 收不到新字节且解析器不在空闲态，则判定半帧并重同步

该模块**不依赖 HAL 与 FreeRTOS**，可在 PC 上脱离硬件做单元测试。

### 3. 中断安全与资源预算
- 中断里只做「拷贝 + 释放信号量」两件确定性的事，所有阻塞操作移出中断上下文
- `USART1_IRQn` / `DMA1_Channel5_IRQn` 的 NVIC 抢占优先级设为 5，
  与 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5` 对齐
  （数值再小一级就不可在中断中调用 FreeRTOS API）
- 资源预算：FreeRTOS 堆 3072 B（heap_4）、三个任务各 512 B 栈（`128 * 4`）、
  环形缓冲 512 B、DMA 缓冲 128 B —— 都逐一核算过；应用层的 `pvPortMalloc`
  与队列操作返回值均有判空/判错处理
- 补全 `HAL_UART_ErrorCallback`：清 ORE 标志并重新武装接收，
  避免溢出标志未清导致 RXNE 不再产生中断、接收永久停摆

## 构建与烧录

```bash
cmake --preset Debug
cmake --build --preset Debug
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -c "program build/Debug/freertos.elf verify reset exit"
```

## 已知待改进

- [ ] 串口波特率 9600 → 115200（当前上报一次统计约需 130ms）
- [ ] 统计上报改用 DMA/中断发送，避免阻塞任务
- [ ] 开启 `configCHECK_FOR_STACK_OVERFLOW` 并实现 hook
- [ ] `MX_FREERTOS_Init()` 里 `osThreadNew` / `osMessageQueueNew` / `osSemaphoreNew`
      的返回值未判空：堆只有 3 KB，创建失败会返回 `NULL`、任务静默不存在。
      计划加创建断言 + 失败时闪灯提示，便于现场定位
- [ ] 整理任务入口函数命名（`KeyTask.c` / `LEDTask.c` 内函数名与注册的任务名交叉）
- [ ] `freertos.ioc` 中的任务优先级与栈大小仍是旧值，与手改过的 `freertos.c`
      不一致 —— 下次用 CubeMX 重新生成代码会被覆盖回去，需要同步
- [ ] LED 消息改为队列值传递，去掉对 `pvPortMalloc` 的依赖
- [ ] 为 `frame_parser` 与 `key_fsm` 补 PC 端单元测试
- [ ] 下一步：CAN 电机控制（bxCAN + TJA1050 双节点实验 → 对接 DJI 电调）

## 目录结构

```
Core/
├── APP/
│   ├── Tasks/     CommandTask.c  KeyTask.c  LEDTask.c  key_fsm.c
│   └── Types/     LEDType.h
├── Inc/           main.h  usart.h  usart_ring.h  frame_parser.h  CRC.h
│                  uart_states.h  key_fsm.h  FreeRTOSConfig.h
└── Src/           main.c  usart.c  usart_ring.c  frame_parser.c  CRC16.c
                   uart_states.c  dma.c  gpio.c  freertos.c  stm32f1xx_it.c
cmake/  Drivers/  Middlewares/
CMakeLists.txt  CMakePresets.json  freertos.ioc  startup_stm32f103xb.s
STM32F103XX_FLASH.ld
```
