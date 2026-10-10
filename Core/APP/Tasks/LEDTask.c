/* LEDTask.c —— LED 驱动任务：消费 LEDQueue，按颜色/状态写 GPIO
 *
 * 入口函数 startLEDTask 与文件名、freertos.ioc 中的 LEDTask 线程三者一致
 * （此前 KeyTask.c / LEDTask.c 两个文件的入口函数名互换过，已修正）。
 */
#include "cmsis_os2.h"
#include "main.h"
#include "FreeRTOS.h"
#include "Types/LEDType.h"


void startLEDTask(void *argument) {
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
                break;                 /* 板载灯处理完不要穿透到 default */
            default:
                break;
        }
        vPortFree(message);

    }
}
