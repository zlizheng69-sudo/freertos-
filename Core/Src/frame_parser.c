/* frame_parser.c */
#include "frame_parser.h"
#include "CRC.h"

typedef enum {
    S_HEAD1 = 0,   /* 等 0xAA */
    S_HEAD2,       /* 等 0x55 */
    S_LEN,         /* 收 LEN */
    S_BODY         /* 收 SEQ + DATA + CRC */
} fp_state_t;

static fp_state_t s_state;
static uint8_t    s_buf[FRAME_MAX_LEN];
static uint16_t   s_idx;        /* 已收字节数 */
static uint16_t   s_total;      /* 本帧期望总长度 */

/* 最近一帧的信息（reset 之后仍然有效，供 accessor 使用） */
static uint16_t   s_last_len;
static uint8_t    s_last_seq;

void frame_parser_reset(void)
{
    s_state = S_HEAD1;
    s_idx   = 0;
    s_total = 0;
}

uint8_t frame_parser_is_idle(void)
{
    return (s_state == S_HEAD1) ? 1u : 0u;
}

fp_result_t frame_parser_feed(uint8_t byte)
{
    fp_result_t result = FP_NONE;

    switch (s_state) {
    case S_HEAD1:
        if (byte == FRAME_HEAD1) {
            s_buf[0] = byte;
            s_idx    = 1;
            s_state  = S_HEAD2;
        }
        break;

    case S_HEAD2:
        if (byte == FRAME_HEAD2) {
            s_buf[1] = byte;
            s_idx    = 2;
            s_state  = S_LEN;
        } else if (byte == FRAME_HEAD1) {
            s_idx = 1;                 /* "AA AA 55" 这种情况，继续等 0x55 */
        } else {
            s_state = S_HEAD1;         /* 同步失败，回去重新找帧头 */
        }
        break;

    case S_LEN:
        if (byte > FRAME_MAX_PAYLOAD) {
            frame_parser_reset();      /* LEN 非法 → 重同步 */
            result = FP_LEN_ERR;
        } else {
            s_buf[2] = byte;
            s_idx    = 3;
            s_total  = (uint16_t)byte + 6u;
            s_state  = S_BODY;
        }
        break;

    case S_BODY:
        s_buf[s_idx++] = byte;         /* s_idx <= 255，s_buf 长度 256，安全 */
        if (s_idx >= s_total) {
            uint16_t crc_rx   = (uint16_t)(s_buf[s_total - 2u]
                                | ((uint16_t)s_buf[s_total - 1u] << 8));
            uint16_t crc_calc = crc16_modbus(&s_buf[2], (uint16_t)(s_total - 4u));

            if (crc_rx == crc_calc) {
                s_last_len = (uint16_t)(s_total - 6u);
                s_last_seq = s_buf[3];
                result = FP_FRAME;
            } else {
                result = FP_CRC_ERR;
            }
            frame_parser_reset();      /* 无论成败，都重新开始找下一帧 */
        }
        break;

    default:
        frame_parser_reset();
        break;
    }

    return result;
}

uint8_t        frame_parser_seq(void)  { return s_last_seq; }
uint16_t       frame_parser_len(void)  { return s_last_len; }
const uint8_t *frame_parser_data(void) { return &s_buf[4]; }