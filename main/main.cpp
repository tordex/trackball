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
#include "ble_init.h"
#include "paw3395.h"
#include "esp_hidd_prf_api.h"
#include "pins.h"
#include <freertos/queue.h>
#include "button.h"

extern uint16_t hid_conn_id;
extern bool sec_conn;

static paw3395 g_sensor;

struct buttons
{
    button              btn;
    button_function_t   func;
};

static button g_buttons[] =
{
    {BTN_ID_LEFT1,      GPIO_NUM_1, BTN_FNC_LEFT      },
    {BTN_ID_LEFT2,      GPIO_NUM_2, BTN_FNC_RIGHT     },
    {BTN_ID_RIGHT1,     GPIO_NUM_3, BTN_FNC_MIDDLE    },
    {BTN_ID_RIGHT2,     GPIO_NUM_4, BTN_FNC_NONE      },
    {BTN_ID_KEY,        GPIO_NUM_5, BTN_FNC_LOCK      },
    {BTN_ID_ENCODER,    GPIO_NUM_6, BTN_FNC_MIDDLE    },
};

static void paw3395_task(void* pvParameters)
{
	int16_t dx, dy;
    uint8_t buttons = 0;
    bool send_report = false;
    int32_t wheel_buffer;
    int32_t ac_pan_buffer;
    button_state_t lock_state = button_state_t::released;
    int8_t wheel = 0;
    int8_t ac_pan = 0;
    bool lock_active = false;
    uint8_t lock_buttons = 0;
    vTaskDelay(pdMS_TO_TICKS(100));
	while (1)
	{
        send_report = false;
        wheel = 0;
        ac_pan = 0;
        if (g_sensor.read_motion(&dx, &dy))
        {
			send_report = true;
        }

        for(int i = 0; i < BTN_ID_MAX; i++)
        {
            switch(g_buttons[i].get_task_id())
            {
                case BTN_FNC_LEFT:
                    g_buttons[i].loop([&buttons, &send_report](button_state_t state) {
                        if(state == button_state_t::pressed)
                        {
                            buttons |= 0x1;
                        } else
                        {
                            buttons &= ~0x1;
                        }
                        send_report = true;
                    });
                    break;
                case BTN_FNC_RIGHT:
                    g_buttons[i].loop([&buttons, &send_report](button_state_t state) {
                        if(state == button_state_t::pressed)
                        {
                            buttons |= 0x2;
                        } else
                        {
                            buttons &= ~0x2;
                        }
                        send_report = true;
                    });
                    break;
                case BTN_FNC_MIDDLE:
                    g_buttons[i].loop([&buttons, &send_report](button_state_t state) {
                        if(state == button_state_t::pressed)
                        {
                            buttons |= 0x4;
                        } else
                        {
                            buttons &= ~0x4;
                        }
                        send_report = true;
                    });
                    break;
                case BTN_FNC_LOCK:
                    g_buttons[i].loop([&lock_state, &wheel_buffer, &ac_pan_buffer](button_state_t state) {
                        lock_state = state;
                        wheel_buffer = 0;
                        ac_pan_buffer = 0;
                    }, [&lock_active, &lock_buttons, buttons, &send_report]() {
                        lock_active = !lock_active;
                        if(lock_active)
                        {
                            lock_buttons = buttons;
                        } else
                        {
                            send_report = true;
                        }
                    });
                    break;
                case BTN_FNC_NONE:
                    break;
            }
        }

        if(send_report)
        {
            if(lock_state == button_state_t::pressed || (lock_active && lock_buttons == 0))
            {
                send_report = false;
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
                //printf("[X] wheel_buffer: %ld dx: %d wheel: %d\n", wheel_buffer, dy, wheel);
                dx = 0;
                dy = 0;
            }
            if(send_report)
            {
                esp_hidd_send_mouse_value(hid_conn_id, lock_active ? lock_buttons : buttons, -dx, dy, wheel, ac_pan);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
	}

}

extern "C" void app_main(void)
{
	esp_log_level_set(HID_DEMO_TAG, ESP_LOG_MAX);
    //esp_log_level_set("BLE_HID_MOUSE", ESP_LOG_MAX);

    // Configure SPI bus
    spi_bus_config_t buscfg = {};
        buscfg.miso_io_num = PIN_NUM_MISO;
        buscfg.mosi_io_num = PIN_NUM_MOSI;
        buscfg.sclk_io_num = PIN_NUM_CLK;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.max_transfer_sz = 32;

    // Initialize SPI bus
    esp_err_t ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
    {
        printf("SPI bus init failed: %d\n", ret);
        return;
    }

    ret = g_sensor.init(SPI3_HOST, PIN_NUM_CS, 600);
    if(ret != ESP_OK)
    {
        printf("Sensor init failed: %d\n", ret);
        return;
    }

	ret = ble_init();
	ESP_ERROR_CHECK(ret);

	xTaskCreatePinnedToCore(&paw3395_task, "paw3395_task", 4096, NULL, 5, NULL, 1);
}
