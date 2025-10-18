#include "nvs_flash.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <freertos/queue.h>

#include "gatt_svr.h"
#include "app_events.h"

static const char* tag = "NimBLEKBD_HIDFUNC";

/*
10 ms is enough time for writing operation, and
a very large amount of time im terms of ble operations*/
#define HID_DEV_BUF_MUTEX_WAIT 10
#define BATTERY_DEFAULT_LEVEL  77

/* notify data buffers */
static uint8_t
	/* mouse: byte 0: bit 0 Button 1, bit 1 Button 2, bit 2 Button 3, bits 4 to 7 zero
	byte 1 : X displacement, byte 2: Y displacement, byte 3: wheel    */
	Mouse_buffer[HIDD_LE_REPORT_MOUSE_SIZE],
	/* battery level */
	Battery_level[] =
		{
			BATTERY_DEFAULT_LEVEL,
},
	/* Feature data - custom data for this device */
	Feature_buffer[] = "olegos";

static struct hid_notify_data
{
	const char* name;
	int			handle_num;		 // handle index from Svc_char_handles
	int			handle_boot_num; // handle num in boot mode
	uint8_t*	buffer;			 // data to send
	size_t		buffer_size;
	bool		can_indicate; // preffered method, because central will response to it
	bool		can_notify;
} Notify_data_reports[] = {
	{.name			  = "mouse",
	 .handle_num	  = HANDLE_HID_MOUSE_REPORT,
	 .handle_boot_num = HANDLE_HID_BOOT_MOUSE_REPORT,
	 .buffer		  = Mouse_buffer,
	 .buffer_size	  = HIDD_LE_REPORT_MOUSE_SIZE,
	 .can_indicate	  = false,
	 .can_notify	  = false},
	{.name			  = "battery level",
	 .handle_num	  = HANDLE_BATTERY_LEVEL,
	 .handle_boot_num = HANDLE_BATTERY_LEVEL,
	 .buffer		  = Battery_level,
	 .buffer_size	  = HIDD_LE_BATTERY_LEVEL_SIZE,
	 .can_indicate	  = false,
	 .can_notify	  = false},
	{.name			  = "feature",
	 .handle_num	  = HANDLE_HID_FEATURE_REPORT,
	 .handle_boot_num = HANDLE_HID_FEATURE_REPORT,
	 .buffer		  = Feature_buffer,
	 .buffer_size	  = HIDD_LE_REPORT_FEATURE,
	 .can_indicate	  = false,
	 .can_notify	  = false},
};

static struct hid_device_data
{
	/* Mutex semaphore for access to this struct */
	SemaphoreHandle_t semaphore;
	bool			  suspended_state;
	bool			  report_mode_boot;
	bool			  connected;
	uint16_t		  conn_handle;
} My_hid_dev = {
	.semaphore		  = 0,
	.connected		  = false,
	.suspended_state  = false,
	.report_mode_boot = false,
};

/* mark report for indicate/notify when central subscribes to service charachetric with report */
void hid_set_notify(uint16_t attr_handle, uint8_t cur_notify, uint8_t cur_indicate)
{
	int report_idx = -1;

	/* find hid_notify_data struct index of reports array for given atribute handle */
	for(int i = 0; i < sizeof(Notify_data_reports) / sizeof(Notify_data_reports[0]); ++i)
	{
		uint16_t current_handle_idx =
			My_hid_dev.report_mode_boot ? Notify_data_reports[i].handle_boot_num : Notify_data_reports[i].handle_num;
		if(attr_handle == Svc_char_handles[current_handle_idx])
		{
			report_idx = i;
			break;
		}
	}
	if(report_idx == -1)
	{
		ESP_LOGW(tag, "%s: attr_handle %04X not found in reports", __FUNCTION__, attr_handle);
	} else
	{
		Notify_data_reports[report_idx].can_indicate = cur_indicate;
		Notify_data_reports[report_idx].can_notify	 = cur_notify;

		ESP_LOGI(tag, "%s: service %s, attr_handle %d, notify %d, indicate %d", __FUNCTION__,
				 Notify_data_reports[report_idx].name, attr_handle, cur_notify, cur_indicate);
	}
}

/* lock report buffers with mutex semaphore */
static int lock_hid_data()
{
	if(!My_hid_dev.semaphore)
	{
		ESP_LOGW(tag, "%s semaphore is NULL", __FUNCTION__);
		return 1;
	}
	if(xSemaphoreTake(My_hid_dev.semaphore, pdMS_TO_TICKS(HID_DEV_BUF_MUTEX_WAIT)) == pdTRUE)
	{
		return 0;
	} else
	{
		ESP_LOGW(tag, "%s: can't lock", __FUNCTION__);
	}

	return 2;
}

/* unlock report buffers locked with mutex semaphore */
static int unlock_hid_data()
{
	if(!My_hid_dev.semaphore)
	{
		ESP_LOGW(tag, "%s semaphore is NULL", __FUNCTION__);
		return 1;
	}
	if(xSemaphoreGive(My_hid_dev.semaphore) == pdTRUE)
	{
		return 0;
	}
	return 2;
}

/* zero all fields on new connection */
void hid_clean_vars(struct ble_gap_conn_desc* desc)
{
	// save semaphore for new connection use
	SemaphoreHandle_t semaphore_saved = My_hid_dev.semaphore;
	int				  rc			  = -1;

	if(semaphore_saved)
	{
		rc = lock_hid_data();
	}

	memset(&My_hid_dev, 0, sizeof(struct hid_device_data));

	for(int i = 0; i < sizeof(Notify_data_reports) / sizeof(Notify_data_reports[0]); ++i)
	{
		Notify_data_reports[i].can_indicate = false;
		Notify_data_reports[i].can_notify	= false;
		switch(Notify_data_reports[i].handle_num)
		{
		case HANDLE_HID_MOUSE_REPORT:
			memset(Notify_data_reports[i].buffer, 0, Notify_data_reports[i].buffer_size);
		}
	}

	if(semaphore_saved)
	{
		My_hid_dev.semaphore = semaphore_saved;
	} else
	{
		My_hid_dev.semaphore = xSemaphoreCreateMutex();
		assert(My_hid_dev.semaphore != NULL);
	}

	My_hid_dev.conn_handle = desc->conn_handle;
	My_hid_dev.connected   = true;

	if(!rc)
	{
		unlock_hid_data();
	}

	send_app_event(APP_EVENT_CONNECTION);
}

void hid_set_disconnected()
{
	My_hid_dev.connected = false;
	send_app_event(APP_EVENT_CONNECTION);
}

bool hid_get_connected()
{
	return My_hid_dev.connected;
}

bool hid_get_rssi(int8_t* out_rssi)
{
	if(My_hid_dev.connected)
	{
		int rc = ble_gap_conn_rssi(My_hid_dev.conn_handle, out_rssi);
		return (rc == 0);
	}
	return false;
}

bool hid_set_suspend(bool need_suspend)
{
	bool last_state			   = My_hid_dev.suspended_state;
	My_hid_dev.suspended_state = need_suspend;
	return last_state;
}

bool hid_set_report_mode(bool is_mode_boot)
{
	bool old_boot				= My_hid_dev.report_mode_boot;
	My_hid_dev.report_mode_boot = is_mode_boot;
	return old_boot;
}

char outbuf[100];

char* print_buf(uint8_t* buf, int buf_size)
{
	outbuf[0] = 0;
	for(int i = 0; i < buf_size; ++i)
	{
		sprintf(outbuf + strlen(outbuf), "%s%02X", i ? " " : "", buf[i]);
	}
	return outbuf;
}

int hid_read_buffer(struct os_mbuf* buf, int handle_num)
{
	int rc		= 0;
	int rep_idx = -1;

	for(int i = 0; i < sizeof(Notify_data_reports) / sizeof(Notify_data_reports[0]); ++i)
	{
		if(Notify_data_reports[i].handle_num == handle_num || Notify_data_reports[i].handle_boot_num == handle_num)
		{
			rep_idx = i;
			break;
		}
	}

	if(rep_idx != -1 && lock_hid_data() == 0)
	{
		rc = os_mbuf_append(buf, Notify_data_reports[rep_idx].buffer, Notify_data_reports[rep_idx].buffer_size);
		unlock_hid_data();

		// ESP_LOGI("", "%s read data: %s", __FUNCTION__,
		//     print_buf(Notify_data_reports[rep_idx].buffer,
		//         Notify_data_reports[rep_idx].buffer_size));
	} else
	{
		if(rep_idx == -1)
			ESP_LOGW(tag, "%s: handle_num %d not found", __FUNCTION__, handle_num);
		return 2;
	}

	return rc;
}

int hid_write_buffer(struct os_mbuf* buf, int handle_num)
{
	int rc		= 0;

	int rep_idx = -1;

	for(int i = 0; i < sizeof(Notify_data_reports) / sizeof(Notify_data_reports[0]); ++i)
	{
		if(Notify_data_reports[i].handle_num == handle_num || Notify_data_reports[i].handle_boot_num == handle_num)
		{
			rep_idx = i;
			break;
		}
	}
	if(rep_idx != -1 && lock_hid_data() == 0)
	{
		if(OS_MBUF_PKTLEN(buf) == Notify_data_reports[rep_idx].buffer_size)
		{
			rc = ble_hs_mbuf_to_flat(buf, Notify_data_reports[rep_idx].buffer, OS_MBUF_PKTLEN(buf), NULL);
		} else
		{
			rc = 4;
		}
		unlock_hid_data();
		if(rc == 0)
		{
			/*if (handle_num == HANDLE_HID_KB_OUT_REPORT) {
				// change LEDs level when Keyboard out report received
				//set_leds(Leds_buffer[0]);
			}*/
		}
	} else
	{
		return 2;
	}

	return rc;
}

/*  send report to central using different ways
	0 - using ble_gattc_indicate_custom     using custom buffer
	1 - using ble_gattc_indicate            to only one connection
	2 - using ble_gatts_chr_updated         to all connected centrals
*/
#define SEND_METHOD_CUSTOM 0
#define SEND_METHOD_STD	   1
#define SEND_METHOD_ALL	   2

#define NOTIFY_METHOD	   SEND_METHOD_STD

/* send report data to central using notify/indicate */
int hid_send_report(int report_handle_num)
{
	/* check semaphore, connection and suspend state */
	if(!My_hid_dev.semaphore || !My_hid_dev.connected || My_hid_dev.suspended_state)
	{
		ESP_LOGI(tag, "%s semaphore %p %d %d", __FUNCTION__, My_hid_dev.semaphore, My_hid_dev.connected,
				 My_hid_dev.suspended_state);
		return 1;
	}

	int report_idx = -1;

	for(int i = 0; i < sizeof(Notify_data_reports) / sizeof(Notify_data_reports[0]); ++i)
	{
		if(report_handle_num == Notify_data_reports[i].handle_num)
		{
			report_idx = i;
			break;
		}
	}

	if(report_idx == -1)
	{
		ESP_LOGW(tag, "%s: Unknown report_handle_num %d", __FUNCTION__, report_handle_num);
		return 2;
	}

	uint16_t send_handle;
	int		 rc = 0;

	if(My_hid_dev.report_mode_boot)
	{
		send_handle = Svc_char_handles[Notify_data_reports[report_idx].handle_boot_num];
	} else
	{
		send_handle = Svc_char_handles[Notify_data_reports[report_idx].handle_num];
	}

	switch(NOTIFY_METHOD)
	{
	case SEND_METHOD_CUSTOM:
		{
			if(lock_hid_data() == 0)
			{
				struct os_mbuf* om = ble_hs_mbuf_from_flat(Notify_data_reports[report_idx].buffer,
														   Notify_data_reports[report_idx].buffer_size);
				unlock_hid_data();

				if(Notify_data_reports[report_idx].can_indicate)
				{
					rc = ble_gattc_indicate_custom(My_hid_dev.conn_handle, send_handle, om);
				} else if(Notify_data_reports[report_idx].can_notify)
				{
					rc = ble_gattc_notify_custom(My_hid_dev.conn_handle, send_handle, om);
				}
			}
			break;
		}

	case SEND_METHOD_STD:
		if(Notify_data_reports[report_idx].can_indicate)
		{
			rc = ble_gattc_indicate(My_hid_dev.conn_handle, send_handle);
		} else if(Notify_data_reports[report_idx].can_notify)
		{
			rc = ble_gattc_notify(My_hid_dev.conn_handle, send_handle);
		}
		break;

	case SEND_METHOD_ALL:
		ble_gatts_chr_updated(send_handle);
		rc = 0;
		break;
	}
	if(rc)
	{
		ESP_LOGE(tag, "%s: Notify error in function", __FUNCTION__);
	}

	return 0;
}

uint8_t hid_battery_level_get(void)
{
	return Battery_level[0];
}

int hid_battery_level_set(uint8_t level)
{
	int rc = 0;

	if(lock_hid_data() == 0)
	{
		if(Battery_level[0] != level)
		{
			Battery_level[0] = level;
		}
		unlock_hid_data();

		rc = hid_send_report(HANDLE_BATTERY_LEVEL);
	} else
	{
		return -2;
	}

	return rc;
}

int hid_mouse_send_report(uint8_t mouse_button, int16_t mickeys_x, int16_t mickeys_y, int8_t wheel, int8_t ac_pan)
{
	if(!hid_get_connected())
		return 1;

	if(lock_hid_data() == 0)
	{
		Mouse_buffer[0] = mouse_button;					// Buttons
		Mouse_buffer[1] = (uint8_t) (mickeys_x & 0xFF); // X Low
		Mouse_buffer[2] = (uint8_t) (mickeys_x >> 8);	// X High
		Mouse_buffer[3] = (uint8_t) (mickeys_y & 0xFF); // Y Low
		Mouse_buffer[4] = (uint8_t) (mickeys_y >> 8);	// Y High
		Mouse_buffer[5] = wheel;						// Wheel
		Mouse_buffer[6] = ac_pan;						// AC Pan

		unlock_hid_data();

		return hid_send_report(HANDLE_HID_MOUSE_REPORT);
	}

	return 1;
}
