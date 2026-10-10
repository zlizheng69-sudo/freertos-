//
// Created by 25725 on 2026/10/2.
//

#ifndef FREERTOS_CRC_H
#define FREERTOS_CRC_H


#include <stdint.h>

/* CRC-16/MODBUS：poly 0x8005（反射实现 0xA001）、init 0xFFFF、输入输出反射、无最终异或
 * 测试向量："123456789" → 0x4B37  ← 先把这个跑通再上板 */
uint16_t crc16_modbus(const uint8_t *data, uint16_t len);


#endif //FREERTOS_CRC_H