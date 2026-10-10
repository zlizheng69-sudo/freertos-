//
// Created by 25725 on 2026/10/2.
//

#ifndef FREERTOS_FRAME_PARSER_H
#define FREERTOS_FRAME_PARSER_H
/* frame_parser.h —— 串口帧解析（纯逻辑，不依赖 HAL / FreeRTOS）
 *
 * 协议：
 *   [0]=AA  [1]=55  [2]=LEN  [3]=SEQ  [4 .. 4+LEN-1]=DATA  [4+LEN]=CRC_L  [5+LEN]=CRC_H
 *   总长 = LEN + 6；CRC16/MODBUS 覆盖 [2 .. 3+LEN]（即 LEN + SEQ + DATA）
 *   LEN = 0 表示无数据帧（本工程约定为"查询统计"）
 */


#include <stdint.h>

#define FRAME_HEAD1        0xAAu
#define FRAME_HEAD2        0x55u
#define FRAME_MAX_PAYLOAD  250u
#define FRAME_MAX_LEN      (FRAME_MAX_PAYLOAD + 6u)     /* 整帧最大 256 字节 */

typedef enum {
    FP_NONE = 0,     /* 还没凑齐完整帧 */
    FP_FRAME,        /* 收到一帧，CRC 正确 */
    FP_CRC_ERR,      /* 帧收齐但 CRC 错 */
    FP_LEN_ERR       /* LEN 非法（超范围） */
} fp_result_t;

void        frame_parser_reset(void);
uint8_t     frame_parser_is_idle(void);      /* 1 = 正在找帧头（可用于半帧超时判断） */
fp_result_t frame_parser_feed(uint8_t byte); /* 每次喂一个字节 */

/* 仅在 frame_parser_feed 返回 FP_FRAME 后、喂下一个字节前有效！ */
uint8_t        frame_parser_seq(void);
uint16_t       frame_parser_len(void);       /* DATA 区长度 */
const uint8_t *frame_parser_data(void);      /* 指向 DATA 区 */


#endif //FREERTOS_FRAME_PARSER_H