/* key_fsm.h —— 按键语义识别（纯逻辑，不依赖 HAL / 不读引脚） */
#ifndef KEY_FSM_H
#define KEY_FSM_H

#include <stdint.h>

typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_SHORT_PRESS,   /* 短按：按下并松开，且在长按阈值内 */
    KEY_EVENT_LONG_PRESS,    /* 长按：按住达到阈值时立即上报一次 */
    KEY_EVENT_DOUBLE_CLICK   /* 双击：双击窗口内第二次按下并松开 */
} key_event_t;

/* 时间参数全部以"采样周期"为单位：改采样率时不用改毫秒值 */
#define KEY_SAMPLE_MS        5u
#define KEY_DEBOUNCE_MS      15u
#define KEY_DEBOUNCE_SAMPLES (KEY_DEBOUNCE_MS / KEY_SAMPLE_MS)   /* 3  */
#define KEY_LONG_MS          1000u
#define KEY_LONG_SAMPLES     (KEY_LONG_MS / KEY_SAMPLE_MS)       /* 200 */
#define KEY_DOUBLE_MS        300u
#define KEY_DOUBLE_SAMPLES   (KEY_DOUBLE_MS / KEY_SAMPLE_MS)     /* 60 */

uint8_t     key_debounce_sample(uint8_t raw_pressed);   /* 1=按下 */
key_event_t key_fsm_update(uint8_t stable_pressed);
void        key_fsm_reset(void);

#endif /* KEY_FSM_H */