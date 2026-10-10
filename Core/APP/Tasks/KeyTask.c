/* KeyTask.c —— 按键任务：读引脚 → 消抖 → 状态机 → 事件分发
*
 * 说明：本文件里的 startLEDTask 是历史遗留名（它其实做按键扫描，
 * 且被 freertos.c 注册成了 "LEDTask" 线程）。M1 先不动命名，避免一次改太多。
 *
 * 关于 EXTI/KeySemaphore：本实现用固定 5 ms 轮询作为时间基准，
 * 不再需要 EXTI 唤醒（信号量停在 1 无副作用）。
 */
#include "cmsis_os2.h"
#include "main.h"
#include "FreeRTOS.h"
#include "Types/LEDType.h"
#include "key_fsm.h"

#define KEY_IS_DOWN()  (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET)

/* 事件 → 动作：只用一颗红灯也能区分三种事件 */
static void key_event_dispatch(key_event_t ev, led_state_t *led_on)
{
    led_t *message;

    switch (ev) {
        case KEY_EVENT_SHORT_PRESS:                        /* 短按：翻转 */
            *led_on = (*led_on == LED_ON) ? LED_OFF : LED_ON;
            break;
        case KEY_EVENT_LONG_PRESS:                         /* 长按：点亮 */
            *led_on = LED_ON;
            break;
        case KEY_EVENT_DOUBLE_CLICK:                       /* 双击：熄灭 */
            *led_on = LED_OFF;
            break;
        default:
            return;                                        /* 无事件 */
    }

    message = pvPortMalloc(sizeof(led_t));
    if (message == NULL) {
        return;                                            /* 堆很小，必须判空 */
    }
    message->color = LED_RED;
    message->state = *led_on;

    if (osMessageQueuePut(LEDQueueHandle, &message, 0, 0) != osOK) {
        vPortFree(message);                                /* 入队失败自己回收 */
    }
}

void startLEDTask(void *argument)
{
    led_state_t led_on = LED_OFF;

    (void)argument;
    key_fsm_reset();

    for (;;) {
        uint8_t raw    = KEY_IS_DOWN() ? 1u : 0u;
        uint8_t stable = key_debounce_sample(raw);   /* 第 1 层：消抖 */
        key_event_t ev = key_fsm_update(stable);     /* 第 2 层：语义识别 */

        key_event_dispatch(ev, &led_on);             /* 第 3 层：动作 */

        osDelay(KEY_SAMPLE_MS);                      /* 固定节拍 = 时间基准 */
    }
}