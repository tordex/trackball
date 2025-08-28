#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_led.h"
#include "esp_log.h"
#include "led_strip_encoder.h"

#define TAG "esp_led"

esp_led::esp_led(gpio_num_t gpio_num, uint32_t resolution) :
        m_gpio_num(gpio_num),
        m_resolution(resolution)
{
    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_tx_channel_config_t tx_chan_config = {};
        tx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT; // select source clock
        tx_chan_config.gpio_num = m_gpio_num;
        tx_chan_config.mem_block_symbols = 64; // increase the block size can make the LED less flickering
        tx_chan_config.resolution_hz = resolution;
        tx_chan_config.trans_queue_depth = 4; // set the number of transactions that can be pending in the background
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &m_led_chan));

    ESP_LOGI(TAG, "Install led strip encoder");
    led_strip_encoder_config_t encoder_config = {
        .resolution = resolution,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &m_led_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(m_led_chan));
}

esp_led::~esp_led()
{

}

void esp_led::set_color(const led_color& color)
{
    if(color != m_color)
    {
        m_color = color;
        rmt_transmit_config_t tx_config = {};
            tx_config.loop_count = 0; // no transfer loop
        uint8_t data[3] = {color.green, color.red, color.blue}; // WS2812 expects GRB order
        ESP_ERROR_CHECK(rmt_transmit(m_led_chan, m_led_encoder, data, sizeof(data), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(m_led_chan, portMAX_DELAY));
    }
}
