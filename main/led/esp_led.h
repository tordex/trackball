#ifndef __ESP_LED_H__
#define __ESP_LED_H__

#include <cstdint>
#include "driver/gpio.h"
#include "driver/rmt_tx.h"

struct led_color
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    led_color() : red(0), green(0), blue(0) {}
    led_color(uint8_t r, uint8_t g, uint8_t b) : red(r), green(g), blue(b) {}

    bool operator==(const led_color& other) const
    {
        return (red == other.red) && (green == other.green) && (blue == other.blue);
    }

    led_color& operator=(const led_color& other)
    {
        if (this != &other)
        {
            red = other.red;
            green = other.green;
            blue = other.blue;
        }
        return *this;
    }
};

class esp_led
{
    gpio_num_t              m_gpio_num;
    uint32_t                m_resolution;
    rmt_channel_handle_t    m_led_chan = NULL;
    rmt_encoder_handle_t    m_led_encoder = NULL;
    led_color               m_color;
public:
    esp_led(gpio_num_t gpio_num, uint32_t resolution);
    ~esp_led();

    void set_color(const led_color& color);
};

#endif /* __ESP_LED_H__ */