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

/**
 * Brief:
 * This example Implemented BLE HID device profile related functions, in which the HID device
 * has 4 Reports (1 is mouse, 2 is keyboard and LED, 3 is Consumer Devices, 4 is Vendor devices).
 * Users can choose different reports according to their own application scenarios.
 * BLE HID profile inheritance and USB HID class.
 */

/**
 * Note:
 * 1. Win10 does not support vendor report , So SUPPORT_REPORT_VENDOR is always set to FALSE, it defines in
 * hidd_le_prf_int.h
 * 2. Update connection parameters are not allowed during iPhone HID encryption, slave turns
 * off the ability to automatically update connection parameters during encryption.
 * 3. After our HID device is connected, the iPhones write 1 to the Report Characteristic Configuration Descriptor,
 * even if the HID encryption is not completed. This should actually be written 1 after the HID encryption is completed.
 * we modify the permissions of the Report Characteristic Configuration Descriptor to `ESP_GATT_PERM_READ |
 * ESP_GATT_PERM_WRITE_ENCRYPTED`. if you got `GATT_INSUF_ENCRYPTION` error, please ignore.
 */

extern uint16_t hid_conn_id;
extern bool sec_conn;

static paw3395 g_sensor;
static QueueHandle_t button_queue;

// Параметры дебаунсинга
#define DEBOUNCE_MS 8 // Время дебаунсинга (10 мс)

// Структура для дебаунсинга
typedef struct 
{
    uint32_t gpio_num;
    int level;
    TickType_t last_change;
    bool stable;
} debounce_t;

// Состояние кнопок и энкодера
static debounce_t button_states[] = {
    {PIN_NUM_LEFT_BUTTON, 1, 0, true},
    {PIN_NUM_RIGHT_BUTTON, 1, 0, true},
    {PIN_NUM_MIDDLE_BUTTON, 1, 0, true},
    {PIN_NUM_BUTTON_X, 1, 0, true}
};


static void paw3395_task(void* pvParameters)
{
	int16_t dx, dy;
    uint8_t buttons = 0;
    bool send_report = false;
    uint32_t gpio_num;
    int32_t wheel_buffer;
    int32_t ac_pan_buffer;
    int8_t wheel = 0;
    int8_t ac_pan = 0;
    vTaskDelay(pdMS_TO_TICKS(100)); // Delay 5ms*/
	while (1)
	{
        send_report = false;
        wheel = 0;
        ac_pan = 0;
        if (g_sensor.read_motion(&dx, &dy))
        {
			send_report = true;
        }

        //if (xQueueReceive(button_queue, &gpio_num, 0)) 
        {
            for (int i = 0; i < 4; i++) 
            {
                //if (button_states[i].gpio_num == gpio_num) 
                {
                    gpio_num = button_states[i].gpio_num;
                    //printf("bnt: %lu\n", gpio_num);
                    int level = gpio_get_level((gpio_num_t) gpio_num);
                    TickType_t now = xTaskGetTickCount();
                    if (level != button_states[i].level) 
                    {
                        button_states[i].level = level;
                        button_states[i].last_change = now;
                        button_states[i].stable = false;
                    } else if (!button_states[i].stable && (now - button_states[i].last_change) >= pdMS_TO_TICKS(DEBOUNCE_MS)) 
                    {
                        button_states[i].stable = true;
                        send_report = true;
                        if (gpio_num == PIN_NUM_LEFT_BUTTON)
                        {
                            buttons = (buttons & ~0x01) | (!level << 0);
                        } else if (gpio_num == PIN_NUM_RIGHT_BUTTON) 
                        {
                            buttons = (buttons & ~0x02) | (!level << 1);
                        } else if (gpio_num == PIN_NUM_MIDDLE_BUTTON) 
                        {
                            buttons = (buttons & ~0x04) | (!level << 2);
                        } else if (gpio_num == PIN_NUM_BUTTON_X) 
                        {
                            buttons = (buttons & ~0x08) | (!level << 3);
                            if(buttons & 0x08)
                            {
                                wheel_buffer = 0;
                                ac_pan_buffer = 0;
                                //printf("X BUTTON\n");
                            }
                        }
                    }
                }
            }
        }

        if(send_report)
        {
            if(buttons & 0x08)
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
                esp_hidd_send_mouse_value(hid_conn_id, buttons, -dx, dy, wheel, ac_pan);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Delay 5ms*/
	}
	
}

// Обработчик прерывания кнопок
static void IRAM_ATTR button_isr_handler(void *arg) 
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(button_queue, &gpio_num, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void buttons_init() 
{
    gpio_install_isr_service(0);

    button_queue = xQueueCreate(20, sizeof(uint32_t));

    gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << PIN_NUM_LEFT_BUTTON) | (1ULL << PIN_NUM_RIGHT_BUTTON) | (1ULL << PIN_NUM_MIDDLE_BUTTON) | (1ULL << PIN_NUM_BUTTON_X),
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_ANYEDGE;
    gpio_config(&io_conf);

    //gpio_isr_handler_add(PIN_NUM_LEFT_BUTTON, button_isr_handler, (void *)PIN_NUM_LEFT_BUTTON);
    //gpio_isr_handler_add(PIN_NUM_RIGHT_BUTTON, button_isr_handler, (void *)PIN_NUM_RIGHT_BUTTON);
    //gpio_isr_handler_add(PIN_NUM_MIDDLE_BUTTON, button_isr_handler, (void *)PIN_NUM_MIDDLE_BUTTON);
    //gpio_isr_handler_add(PIN_NUM_BUTTON_X, button_isr_handler, (void *)PIN_NUM_BUTTON_X);
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

    ret = g_sensor.init(SPI3_HOST, PIN_NUM_CS, 800);
    if(ret != ESP_OK)
    {
        printf("Sensor init failed: %d\n", ret);
        return;
    }
	
	ret = ble_init();
	ESP_ERROR_CHECK(ret);

    buttons_init();

	xTaskCreatePinnedToCore(&paw3395_task, "paw3395_task", 4096, NULL, 5, NULL, 1);
}
