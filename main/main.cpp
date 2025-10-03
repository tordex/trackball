/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "paw3395.h"
#include "pins.h"
#include <freertos/queue.h>
#include "button.h"
#include "esp_led.h"
#include "hid_func.h"
#include "nvs_flash.h"

static paw3395 g_sensor;

static const char* TAG = "MAIN";
extern "C" void ble_init();

/* for nvs_storage*/
#define LOCAL_NAMESPACE "storage"
nvs_handle_t Nvs_storage_handle = 0;

QueueHandle_t g_app_events = nullptr;

struct buttons
{
    button              btn;
    button_function_t   func;
};

const led_color COLOR_OFF      = { 0,   0,   0   };
const led_color COLOR_RED      = { 255, 0,   0   };
const led_color COLOR_GREEN    = { 0,   255, 0   };
const led_color COLOR_BLUE     = { 0,   0,   255 };
const led_color COLOR_YELLOW   = { 255, 255, 0   };
const led_color COLOR_CYAN     = { 0,   255, 255 };
const led_color COLOR_MAGENTA  = { 255, 0,   255 };
const led_color COLOR_WHITE    = { 255, 255, 255 };

static void paw3395_task(void* pvParameters)
{
    button g_buttons[] =
    {
        {BTN_ID_LEFT1,      GPIO_NUM_1, BTN_FNC_LEFT      },
        {BTN_ID_LEFT2,      GPIO_NUM_2, BTN_FNC_RIGHT     },
        {BTN_ID_RIGHT1,     GPIO_NUM_3, BTN_FNC_MIDDLE    },
        {BTN_ID_RIGHT2,     GPIO_NUM_4, BTN_FNC_NONE      },
        {BTN_ID_KEY,        GPIO_NUM_5, BTN_FNC_LOCK      },
        {BTN_ID_ENCODER,    GPIO_NUM_6, BTN_FNC_MIDDLE    },
    };


    uint8_t buttons = 0;
    button_state_t lock_state = button_state_t::released;
    bool lock_active = false;
    uint8_t lock_buttons = 0;
    esp_led led(GPIO_NUM_21, 4000000); // WS2812 on GPIO21 with 4KHz RMT clock
    led.set_color(COLOR_BLUE); // Blue

    auto ret = g_sensor.init(SPI3_HOST, PIN_NUM_CS, PIN_NUM_MOTION, 600,
        [&lock_active, &lock_buttons, &buttons, &lock_state](int16_t dx, int16_t dy)
        {
            bool send_report = false;
            int8_t wheel = 0;
            int8_t ac_pan = 0;
            static int32_t wheel_buffer = 0;
            static int32_t ac_pan_buffer = 0;
            if(lock_state == button_state_t::pressed || (lock_active && lock_buttons == 0))
            {
                wheel_buffer += dy;
                ac_pan_buffer += dx;
                if(wheel_buffer > 100)
                {
                    wheel = -1;
                    send_report = true;
                    wheel_buffer = 0;
                } else if(wheel_buffer < -100)
                {
                    wheel = 1;
                    send_report = true;
                    wheel_buffer = 0;
                }
                if(ac_pan_buffer > 100)
                {
                    ac_pan = -1;
                    send_report = true;
                    ac_pan_buffer = 0;
                } else if(ac_pan_buffer < -100)
                {
                    ac_pan = 1;
                    send_report = true;
                    ac_pan_buffer = 0;
                }
                dx = 0;
                dy = 0;
            } else
            {
                wheel_buffer = 0;
                ac_pan_buffer = 0;
                send_report = true;
            }
            if(send_report)
            {
                hid_mouse_send_report(lock_active ? lock_buttons : buttons, -dx, dy, wheel, ac_pan);
            }
        });
    if(ret != ESP_OK)
    {
        printf("Sensor init failed: %d\n", ret);
        return;
    }

    auto left_on_state_changed = [&buttons, &lock_state, &lock_active, &lock_buttons](button_state_t state)
    {
        if(state == button_state_t::pressed)
        {
            buttons |= 0x1;
        } else
        {
            buttons &= ~0x1;
        }
        if(!lock_active)
        {
            uint32_t event_id = 1;
            xQueueSend(g_app_events, &event_id, portMAX_DELAY);
        }
    };

    auto right_on_state_changed = [&buttons, &lock_state, &lock_active, &lock_buttons](button_state_t state)
    {
        if(state == button_state_t::pressed)
        {
            buttons |= 0x2;
        } else
        {
            buttons &= ~0x2;
        }
        if(!lock_active)
        {
            uint32_t event_id = 1;
            xQueueSend(g_app_events, &event_id, portMAX_DELAY);
        }
    };

    auto middle_on_state_changed = [&buttons, &lock_state, &lock_active, &lock_buttons](button_state_t state)
    {
        if(state == button_state_t::pressed)
        {
            buttons |= 0x4;
        } else
        {
            buttons &= ~0x4;
        }
        if(!lock_active)
        {
            uint32_t event_id = 1;
            xQueueSend(g_app_events, &event_id, portMAX_DELAY);
        }
    };

    auto lock_on_state_changed = [&lock_state](button_state_t state)
    {
        lock_state = state;
        uint32_t event_id = 0;
        xQueueSend(g_app_events, &event_id, portMAX_DELAY);
    };

    auto lock_on_click = [&lock_active, &lock_buttons, &buttons]()
    {
        uint32_t event_id;
        lock_active = !lock_active;
        if(lock_active)
        {
            lock_buttons = buttons;
        } else
        {
            event_id = 1;
            xQueueSend(g_app_events, &event_id, portMAX_DELAY);
        }
        event_id = 0;
        xQueueSend(g_app_events, &event_id, portMAX_DELAY);
    };

    for(int i = 0; i < BTN_ID_MAX; i++)
    {
        switch(g_buttons[i].get_task_id())
        {
            case BTN_FNC_LEFT:
                g_buttons[i].set_cb_on_state_changed(left_on_state_changed);
                break;
            case BTN_FNC_RIGHT:
                g_buttons[i].set_cb_on_state_changed(right_on_state_changed);
                break;
            case BTN_FNC_MIDDLE:
                g_buttons[i].set_cb_on_state_changed(middle_on_state_changed);
                break;
            case BTN_FNC_LOCK:
                g_buttons[i].set_cb_on_state_changed(lock_on_state_changed);
                g_buttons[i].set_cb_on_click(lock_on_click);
                break;
            case BTN_FNC_NONE:
                break;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    led_color next_color;
	while (true)
	{
        uint32_t event_id;
        if (xQueueReceive(g_app_events, &event_id, portMAX_DELAY))
        {
            if(lock_state == button_state_t::pressed || lock_active)
            {
                if(lock_active)
                    next_color = COLOR_BLUE;
                else
                    next_color = COLOR_CYAN;
            } else
            {
                next_color = hid_get_connected() ? COLOR_GREEN : COLOR_BLUE;
            }

            if(event_id == 1) // Buttons
            {
                hid_mouse_send_report(lock_active ? lock_buttons : buttons, 0, 0, 0, 0);
            }

            led.set_color(next_color);
        }
	}

}

extern "C" void app_main(void)
{
	esp_log_level_set("NimBLE", ESP_LOG_WARN);
	//esp_log_level_set(HID_DEMO_TAG, ESP_LOG_MAX);
    //esp_log_level_set("BLE_HID_MOUSE", ESP_LOG_MAX);

    gpio_install_isr_service(0);

    g_app_events = xQueueCreate(10, sizeof(uint32_t));

    /* Initialize NVS — it is used to store PHY calibration data and Nimble bonding data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK( nvs_open(LOCAL_NAMESPACE, NVS_READWRITE, &Nvs_storage_handle) );

    // Configure SPI bus
    spi_bus_config_t buscfg = {};
        buscfg.miso_io_num = PIN_NUM_MISO;
        buscfg.mosi_io_num = PIN_NUM_MOSI;
        buscfg.sclk_io_num = PIN_NUM_CLK;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.max_transfer_sz = 32;

    // Initialize SPI bus
    ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
    {
        printf("SPI bus init failed: %d\n", ret);
        return;
    }

    ble_init();
    ESP_LOGI(TAG, "BLE init ok");

	xTaskCreatePinnedToCore(&paw3395_task, "paw3395_task", 4096, NULL, 5, NULL, 1);
}
