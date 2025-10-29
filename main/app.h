#pragma once

#include "battery.h"
#include "paw3395.h"
#include "button.h"
#include "ui.h"
#include "pins.h"
#include "timer.h"
#include "nvs_flash.h"

enum app_state_t
{
	APP_STATE_DEFAULT,		// Default state. Normal operation
	APP_STATE_SCROLL_HOLD,	// Scroll button is held. Send scroll events on motion.
	APP_STATE_SCROLL_LOCK,	// Scroll lock active (after scroll button click). Send scroll events on motion.
	APP_STATE_LOCK_BUTTONS, // Pressed buttons are locked (after config button click)
};

enum button_function_t
{
	BTN_FNC_NONE,
	BTN_FNC_LEFT,
	BTN_FNC_RIGHT,
	BTN_FNC_MIDDLE,
};

enum sensor_mode_t
{
	SENSOR_MODE_HIGH_PERFORMANCE,
	SENSOR_MODE_LOW_POWER,
	SENSOR_MODE_OFFICE,
	SENSOR_MODE_GAMING,
};

struct app_config
{
	uint8_t	 btn1_func			= BTN_FNC_LEFT;
	uint8_t	 btn2_func			= BTN_FNC_RIGHT;
	uint8_t	 btn3_func			= BTN_FNC_MIDDLE;
	uint8_t	 btn4_func			= BTN_FNC_NONE;
	uint8_t	 scroll_sensitivity = 100;
	uint16_t dpi				= 600;
	uint8_t	 sensor_mode		= SENSOR_MODE_HIGH_PERFORMANCE;
};

class app
{
private:
	battery		 m_battery;
	paw3395		 m_sensor;
	button		 m_btn_1	  = {PIN_BTN1};		  // Button 1 (left-top)
	button		 m_btn_2	  = {PIN_BTN2};		  // Button 2	(left-bottom)
	button		 m_btn_3	  = {PIN_BTN3};		  // Button 3 (right-top)
	button		 m_btn_4	  = {PIN_BTN4};		  // Button 4 (right-bottom)
	button		 m_btn_scroll = {PIN_BTN_SCROLL}; // Scroll button
	button		 m_btn_cfg	  = {PIN_BTN_CFG};	  // Configuration button
	trackball_ui m_ui;
	app_config	 m_config;
	app_state_t	 m_app_state			= APP_STATE_DEFAULT;

	uint8_t m_buttons					= 0;
	uint8_t m_locked_buttons			= 0;

	// for nvs_storage
	const char*	 m_nvs_namespace		= "storage";
	nvs_handle_t m_nvs_handle			= 0;

	i2c_master_bus_handle_t m_h_i2c_bus = nullptr;
	i2c_master_dev_handle_t m_h_i2c_dev = nullptr;

	timer m_connection_state_timer{"connection_state"};
public:
	app();
	~app() = default;

	void init();
	void deinit();

	void on_connection_changed();

private:
	void apply_config();
	void sensor_motion_callback(int16_t dx, int16_t dy);
	void on_btn_cfg_state_changed(button_state_t state);
	void on_btn_cfg_clicked();
	void on_btn_scroll_state_changed(button_state_t state);
	void on_btn_scroll_clicked();
	void on_update_connection_state();
	void on_battery_state_changed(int voltage, int level);
	void apply_button_function(button_state_t state, button_function_t func);

	void set_app_state(app_state_t state);
	void send_report(int16_t dx = 0, int16_t dy = 0, int8_t wheel = 0, int8_t ac_pan = 0);

	uint8_t get_report_buttons() const
	{
		return m_app_state == APP_STATE_LOCK_BUTTONS ? m_locked_buttons : m_buttons;
	}
};
