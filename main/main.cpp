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

paw3395 g_sensor;


void hid_demo_task(void* pvParameters)
{
	while(1)
	{
		vTaskDelay(2000 / portTICK_PERIOD_MS);
/*		if(sec_conn)
		{
            ESP_LOGI(HID_DEMO_TAG, "Send the 10, 10");
            for(int i = 0; i < 10; i++)
            {
                esp_hidd_send_mouse_value(hid_conn_id, 0, 10, 10);
                //vTaskDelay(100 / portTICK_PERIOD_MS);
            }
			vTaskDelay(1000 / portTICK_PERIOD_MS);
            ESP_LOGI(HID_DEMO_TAG, "Send the -10, -10");
            for(int i = 0; i < 10; i++)
            {
                esp_hidd_send_mouse_value(hid_conn_id, 0, -10, -10);
                //vTaskDelay(10 / portTICK_PERIOD_MS);
            }

			ESP_LOGI(HID_DEMO_TAG, "Send the volume");
			send_volum_up = true;
			// uint8_t key_vaule = {HID_KEY_A};
			// esp_hidd_send_keyboard_value(hid_conn_id, 0, &key_vaule, 1);
			esp_hidd_send_mouse_value(hid_conn_id, 0, 100, 0);
			vTaskDelay(3000 / portTICK_PERIOD_MS);
			if(send_volum_up)
			{
				send_volum_up = false;
				esp_hidd_send_mouse_value(hid_conn_id, 0, 100, 0);
				esp_hidd_send_mouse_value(hid_conn_id, 0, -100, 0);
				vTaskDelay(3000 / portTICK_PERIOD_MS);
				esp_hidd_send_mouse_value(hid_conn_id, 0, 100, 0);
			}
		}*/
	}
}

static void paw3395_task(void* pvParameters)
{
	int16_t dx, dy;
    vTaskDelay(pdMS_TO_TICKS(100)); // Delay 5ms*/
	while (1)
	{
        if (g_sensor.read_motion(&dx, &dy))
        {
			//printf("dx: %hd dy: %hd\n", -dx, dy);
			esp_hidd_send_mouse_value(hid_conn_id, 0, (int8_t) -dx, (int8_t) dy);
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Delay 5ms*/
	}
	
}

extern "C" void app_main(void)
{
	esp_log_level_set(HID_DEMO_TAG, ESP_LOG_MAX);

    // Configure SPI bus
    spi_bus_config_t buscfg = {};
        buscfg.miso_io_num = PIN_NUM_MISO;
        buscfg.mosi_io_num = PIN_NUM_MOSI;
        buscfg.sclk_io_num = PIN_NUM_CLK;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.max_transfer_sz = 32;

    // Initialize SPI bus
    esp_err_t ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_DISABLED);
    if (ret != ESP_OK)
    {
        printf("SPI bus init failed: %d\n", ret);
        return;
    }

    ret = g_sensor.init(SPI3_HOST, PIN_NUM_CS, 1800);
    if(ret != ESP_OK)
    {
        printf("Sensor init failed: %d\n", ret);
        return;
    }
	
	ret = ble_init();
	ESP_ERROR_CHECK(ret);

	xTaskCreate(&paw3395_task, "paw3395_task", 4096, NULL, 5, NULL);
}
