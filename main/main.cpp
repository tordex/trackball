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
#include "hid_func.h"
#include "app.h"

app* g_app = nullptr;

void hid_on_connection_changed()
{
	g_app->on_connection_changed();
}

static void app_task(void* pvParameters)
{
	g_app = new app();
	g_app->init();
	for(;;)
	{
		vTaskDelay(portMAX_DELAY);
	}
}

extern "C" void app_main(void)
{
	esp_log_level_set("NimBLE", ESP_LOG_WARN);
	// esp_log_level_set(HID_DEMO_TAG, ESP_LOG_MAX);
	// esp_log_level_set("BLE_HID_MOUSE", ESP_LOG_MAX);

	gpio_install_isr_service(0);

	xTaskCreatePinnedToCore(&app_task, "app", 4096, NULL, 5, NULL, 1);
}
