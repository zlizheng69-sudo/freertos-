#ifndef UART_STATS_H
#define UART_STATS_H

#include <stdint.h>

/* 统计快照：一次取走所有计数，方便组成一行上报 */
typedef struct {
    uint32_t ok;        /* CRC 通过并已执行的帧数 */
    uint32_t crc_err;   /* CRC 校验失败的帧数 */
    uint32_t len_err;   /* LEN 字段非法的次数 */
    uint32_t overrun;   /* 环形缓冲满而被丢弃的字节数 */
    uint32_t resync;    /* 帧间超时导致的重同步次数 */
    uint32_t uart_err;  /* HAL 错误回调次数（ORE/FE/NE/PE） */
} uart_stats_t;

/* ---- 写入接口：中断侧只用 uart_stat_overrun / uart_stat_uart_err ---- */
void uart_stat_ok(void);                  /* 收帧成功（任务侧） */
void uart_stat_crc_err(void);             /* CRC 错（任务侧） */
void uart_stat_len_err(void);             /* LEN 非法（任务侧） */
void uart_stat_resync(void);              /* 超时重同步（任务侧） */
void uart_stat_overrun(uint16_t lost);    /* 缓冲溢出丢弃 lost 字节（中断侧） */
void uart_stat_uart_err(void);            /* UART 错误（中断侧） */

/* ---- 读取接口 ---- */
void uart_stats_get(uart_stats_t *out);
void uart_stats_reset(void);

#endif /* UART_STATS_H */