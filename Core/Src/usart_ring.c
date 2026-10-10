/* usart_ring.c —— 单生产者(中断) / 单消费者(任务) 环形缓冲
 *
 * 并发模型（决定了为什么不需要加锁）：
 *   s_head 只被 ring_write 修改（中断上下文）
 *   s_tail 只被 ring_read  修改（任务上下文）
 *   → 两个"指针"各自只有一个写者，读写都是单个 16 位访问（Cortex-M3 上原子）
 *
 * 容量约定：RING_SIZE 必须是 2 的幂；
 *   留 1 个空位用于区分"空"和"满"，所以有效容量 = RING_SIZE - 1
 */
#include "usart_ring.h"
#include <string.h>          /* memcpy */
#include "main.h"

#define RING_SIZE   512u                          /* 2 的幂 */
#define RING_MASK   (RING_SIZE - 1u)

static uint8_t           s_buf[RING_SIZE];
static volatile uint16_t s_head;                  /* 写入位置（ISR 改） */
static volatile uint16_t s_tail;                  /* 读出位置（任务改） */

void ring_init(void)
{
    s_head = 0;
    s_tail = 0;
}

/* 当前可读字节数。无符号回绕运算，head 绕回时依然正确 */
uint16_t ring_count(void)
{
    return (uint16_t)((s_head - s_tail) & RING_MASK);
}

/* 写入：返回"实际写进去多少字节"（缓冲满时 < len，剩下的由调用者计数丢弃） */
uint16_t ring_write(const uint8_t *data, uint16_t len)
{
    uint16_t free_cnt;
    uint16_t n;

    if ((data == 0) || (len == 0u)) {
        return 0;
    }

    /* 剩余空间：容量 - 1 个保留位 - 已存数据 */
    free_cnt = (uint16_t)(RING_SIZE - 1u - ring_count());
    if (len > free_cnt) {
        len = free_cnt;                     /* 只写放得下的部分，绝不覆盖未读数据 */
    }
    if (len == 0u) {
        return 0;
    }

    /* 分两段拷贝，处理回绕 */
    n = (uint16_t)(RING_SIZE - s_head);     /* head 到缓冲区末尾的连续空间 */
    if (n > len) {
        n = len;
    }
    memcpy(&s_buf[s_head], data, n);        /* 第一段：head..末尾 */
    if (len > n) {
        memcpy(&s_buf[0], &data[n], (size_t)(len - n));   /* 第二段：绕回开头 */
    }

    s_head = (uint16_t)((s_head + len) & RING_MASK);
    return len;
}

/* 读出：返回"实际读出多少字节"（缓冲空时返回 0） */
uint16_t ring_read(uint8_t *data, uint16_t max)
{
    uint16_t cnt;
    uint16_t n;

    if ((data == 0) || (max == 0u)) {
        return 0;
    }

    cnt = ring_count();
    if (cnt > max) {
        cnt = max;
    }
    if (cnt == 0u) {
        return 0;
    }

    n = (uint16_t)(RING_SIZE - s_tail);
    if (n > cnt) {
        n = cnt;
    }
    memcpy(data, &s_buf[s_tail], n);
    if (cnt > n) {
        memcpy(&data[n], &s_buf[0], (size_t)(cnt - n));
    }

    s_tail = (uint16_t)((s_tail + cnt) & RING_MASK);
    return cnt;
}