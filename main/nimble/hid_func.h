
#ifndef H_HID_FUNC_
#define H_HID_FUNC_

#include "host/ble_gap.h"

#ifdef __cplusplus
extern "C"
{
#endif

	void hid_clean_vars(struct ble_gap_conn_desc* desc);
	void hid_set_disconnected();
	bool hid_get_connected();
	bool hid_get_rssi(int8_t* out_rssi);
	void hid_set_notify(uint16_t attr_handle, uint8_t cur_notify, uint8_t cur_indicate);
	bool hid_set_suspend(bool need_suspend);
	bool hid_set_report_mode(bool boot_mode);
	void hid_on_connection_changed(); // this function must be externaly implemented

	uint8_t hid_battery_level_get(void);

	int hid_battery_level_set(uint8_t level);
	int hid_mouse_send_report(uint8_t mouse_button, int16_t mickeys_x, int16_t mickeys_y, int8_t wheel, int8_t ac_pan);

	int hid_write_buffer(struct os_mbuf* buf, int handle_num);

	int hid_read_buffer(struct os_mbuf* buf, int handle_num);

#ifdef __cplusplus
}
#endif

#endif
