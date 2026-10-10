/* CommandTask.c —— 串口帧处理任务
 *
 * 数据流：DMA/IDLE 中断 → ring_write → 信号量 → 本任务 ring_read → frame_parser → 执行
 */
#include <stdio.h>
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "usart.h"
#include "Types/LEDType.h"
#include "usart_ring.h"
#include "frame_parser.h"
#include "uart_states.h"

#define RX_CHUNK_SIZE    64u    /* 每次从环形缓冲取多少字节 */
#define RX_IDLE_TIMEOUT  20u    /* 空闲超过 20 ms 且有半帧 → 重同步 */

static void uart_report_stats(void);     /* 前向声明 */

/* ---- 执行一条 LED 命令 ---- */
static void led_post(uint8_t color_code, uint8_t state_code)
{
    led_t *message;

    if ((color_code < 1u) || (color_code > 4u)) {
        uart_stat_len_err();              /* 颜色值非法（帧内容错误） */
        return;
    }

    message = pvPortMalloc(sizeof(led_t));
    if (message == 0) {
        return;                           /* 堆很小，失败就放弃 */
    }
    message->color = (led_color_t)(color_code - 1u);
    message->state = (state_code != 0u) ? LED_ON : LED_OFF;

    if (osMessageQueuePut(LEDQueueHandle, &message, 0, 0) != osOK) {
        vPortFree(message);
    }
}

/* ---- 处理一整帧 ---- */
static void handle_frame(void)
{
    uint16_t       len  = frame_parser_len();
    const uint8_t *data = frame_parser_data();
    uint16_t       i;

    if (len == 0u) {                      /* 约定：LEN==0 → 查询统计 */
        uart_report_stats();
        return;
    }

    for (i = 0; (uint16_t)(i + 1u) < len; i += 2u) {     /* DATA = (颜色, 状态) 对 */
        led_post(data[i], data[i + 1u]);
    }
}

/* ---- 统计上报（阻塞发送约 130 ms @9600，环形缓冲能吸收） ---- */
static void uart_report_stats(void)
{
    uart_stats_t st;
    char         line[128];
    int          n;

    uart_stats_get(&st);
    n = snprintf(line, sizeof(line),
                 "STAT ok=%lu crc=%lu len=%lu ovr=%lu resync=%lu err=%lu seq=%u\r\n",
                 (unsigned long)st.ok,       (unsigned long)st.crc_err,
                 (unsigned long)st.len_err,  (unsigned long)st.overrun,
                 (unsigned long)st.resync,   (unsigned long)st.uart_err,
                 (unsigned)frame_parser_seq());
    if (n > 0) {
        HAL_UART_Transmit(&huart1, (const uint8_t *)line, (uint16_t)n, 100u);
    }
}

void StartCommandTask(void *argument)
{
    uint8_t chunk[RX_CHUNK_SIZE];

    (void)argument;

    ring_init();                 /* 先准备好缓冲，再开接收 */
    frame_parser_reset();
    usart1_receive_start();

    for (;;) {
        /* 等"有新数据"；超时说明线路已空闲，用来发现半帧 */
        if (osSemaphoreAcquire(uartSemHandle, RX_IDLE_TIMEOUT) != osOK) {
            if (frame_parser_is_idle() == 0u) {
                uart_stat_resync();          /* 半帧超时 → 重同步 */
                frame_parser_reset();
            }
            continue;
        }

        /* 把环形缓冲抽干：这一批里可能有 0 个 / 多个 / 半个帧 */
        for (;;) {
            uint16_t n = ring_read(chunk, (uint16_t)sizeof(chunk));
            uint16_t i;

            if (n == 0u) {
                break;
            }
            for (i = 0; i < n; i++) {
                switch (frame_parser_feed(chunk[i])) {
                case FP_FRAME:   uart_stat_ok();    handle_frame(); break;
                case FP_CRC_ERR: uart_stat_crc_err();                 break;
                case FP_LEN_ERR: uart_stat_len_err();                 break;
                default:                                              break;
                }
            }
        }
    }
}