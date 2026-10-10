//
// Created by 25725 on 2026/10/2.
//

#ifndef FREERTOS_USART_RING_H
#define FREERTOS_USART_RING_H

#include "main.h"
void ring_init(void);
uint16_t ring_write(const uint8_t* data, uint16_t len);
uint16_t ring_read(uint8_t* data, uint16_t max);
uint16_t ring_count(void);

#endif //FREERTOS_USART_RING_H