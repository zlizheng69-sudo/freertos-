#include "key_fsm.h"


uint8_t key_debounce_sample(uint8_t raw_pressed) {
    static uint8_t stable = 0;
    static uint8_t count =0;
    if (raw_pressed == stable) {
        count = 0;
    } else {
        count++;
        if (count >= KEY_DEBOUNCE_SAMPLES) {
            stable = raw_pressed;
        }
    }
    return stable;
}


typedef enum { K_IDLE = 0, K_DOWN, K_WAIT } kstate_t;

static kstate_t s_state;
static uint16_t s_ticks;      /* 当前状态内的采样计数 */
static uint8_t  s_long_sent;  /* 本次按下期间 LONG 是否已上报 */
static uint8_t  s_is_second;  /* 本次按下是否为"第二击" */



void key_fsm_reset(void) {
    s_state = K_IDLE;
    s_ticks = 0;
    s_long_sent = 0;
    s_is_second = 0;
}

key_event_t key_fsm_update(uint8_t pressed  ) {
    key_event_t event = KEY_EVENT_NONE;

    switch (s_state) {
        case K_IDLE:
            if (pressed) {
                s_ticks = 0;
                s_long_sent = 0;
                s_is_second = 0;
                s_state = K_DOWN;
            }
            break;
        case K_DOWN:
            s_ticks ++;
            if (!s_long_sent && s_ticks >= KEY_LONG_SAMPLES) {
                s_long_sent = 1;
                event = KEY_EVENT_LONG_PRESS;
            }
            if (!pressed) {
                if (s_long_sent) {
                    s_state = K_IDLE;
                }
                else if (s_is_second) {
                    event = KEY_EVENT_DOUBLE_CLICK;
                    s_state = K_IDLE;
                }
                else {
                    s_ticks = 0;
                    s_state = K_WAIT;
                }
            }
            break;
        case K_WAIT:
            s_ticks++;
            if (pressed) {                       /* 窗口内又按下 */
                s_is_second = 1;
                s_long_sent = 0;
                s_ticks = 0;
                s_state = K_DOWN;
            } else if (s_ticks >= KEY_DOUBLE_SAMPLES) {
                event = KEY_EVENT_SHORT_PRESS;      /* 窗口超时 → 确认短按 */
                s_state = K_IDLE;
            }
            break;

        default:
            key_fsm_reset();
            break;

    }
    return event;
}


