typedef enum {
    LED_RED   = 0,
    LED_GREEN = 1,
    LED_BLUE  = 2,
    LED_BOARD = 3      /* PC13 板载灯（低电平点亮！） */
} led_color_t;

typedef enum {
    LED_OFF = 0,
    LED_ON = 1
} led_state_t;
typedef struct {
    led_color_t color;
    led_state_t state;
} led_t;
