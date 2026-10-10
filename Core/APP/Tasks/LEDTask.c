#include "cmsis_os2.h"
#include "main.h"
#include "FreeRTOS.h"
#include "Types/LEDType.h"


void StartKeyTask(void *argument) {
    for (;;) {
        led_t *message ;
        osMessageQueueGet(LEDQueueHandle, &message, 0, osWaitForever);
        switch (message -> color) {
            case LED_RED:
                HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, message -> state ? GPIO_PIN_SET : GPIO_PIN_RESET);
                break;
            case LED_GREEN:
                HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, message -> state ? GPIO_PIN_SET : GPIO_PIN_RESET);
                break;
            case LED_BLUE:
                HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, message -> state ? GPIO_PIN_SET : GPIO_PIN_RESET);
                break;
            case LED_BOARD:                 /* ← 新增 */
                /* PC13 板载灯是"低电平点亮"，所以高低电平要反过来 */
                HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin,
                                  (message->state == LED_ON) ? GPIO_PIN_RESET : GPIO_PIN_SET);
            default:
                break;
        }
        vPortFree(message);

    }
}
