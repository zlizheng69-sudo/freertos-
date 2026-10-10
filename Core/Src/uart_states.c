/* uart_stats.c —— 串口接收统计计数
*
 * 并发规则（很重要，决定了这里为什么不需要加锁）：
 *   1. 每个计数器只有一个"写者"：要么只在中断里写，要么只在任务里写
 *   2. Cortex-M3 上对齐的 32 位读/写是单条 LDR/STR 指令，本身是原子的
 *      → 任务读、中断写，不会读到"半个数"
 *   3. volatile：禁止编译器把计数器缓存进寄存器
 *      （否则 -O2 下任务可能永远读到 0，这类 bug 在 -O0 调试时还不出现）
 */
#include "uart_states.h"

static volatile uint32_t s_ok;
static volatile uint32_t s_crc_err;
static volatile uint32_t s_len_err;
static volatile uint32_t s_overrun;
static volatile uint32_t s_resync;
static volatile uint32_t s_uart_err;

void uart_stat_ok(void)       { s_ok++; }
void uart_stat_crc_err(void)  { s_crc_err++; }
void uart_stat_len_err(void)  { s_len_err++; }
void uart_stat_resync(void)   { s_resync++; }
void uart_stat_uart_err(void) { s_uart_err++; }

void uart_stat_overrun(uint16_t lost)
{
    s_overrun += lost;        /* 只有中断侧调用它，不存在"丢更新" */
}

void uart_stats_get(uart_stats_t *out)
{
    if (out == 0) {
        return;
    }
    out->ok       = s_ok;
    out->crc_err  = s_crc_err;
    out->len_err  = s_len_err;
    out->overrun  = s_overrun;
    out->resync   = s_resync;
    out->uart_err = s_uart_err;
}

void uart_stats_reset(void)
{
    s_ok       = 0;
    s_crc_err  = 0;
    s_len_err  = 0;
    s_overrun  = 0;
    s_resync   = 0;
    s_uart_err = 0;
}