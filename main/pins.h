#ifndef __PINS__H__
#define __PINS__H__

#define PIN_NUM_MISO            GPIO_NUM_13
#define PIN_NUM_MOSI            GPIO_NUM_11
#define PIN_NUM_CLK             GPIO_NUM_12
#define PIN_NUM_CS              GPIO_NUM_10
#define PIN_NUM_MOTION          GPIO_NUM_9

enum buttons_ids_t
{
    BTN_ID_LEFT1,
    BTN_ID_LEFT2,
    BTN_ID_RIGHT1,
    BTN_ID_RIGHT2,
    BTN_ID_KEY,
    BTN_ID_ENCODER,
    BTN_ID_MAX
};

enum button_function_t
{
    BTN_FNC_NONE,
    BTN_FNC_LEFT,
    BTN_FNC_RIGHT,
    BTN_FNC_MIDDLE,
    BTN_FNC_LOCK,
};

#endif // __PINS__H__