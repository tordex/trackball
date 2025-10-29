#include "freertos/FreeRTOS.h"
#include "app.h"
#include "esp_log.h"
#include "hid_func.h"
#include "nvs_flash.h"
#include "driver/i2c_master.h"
#include "pins.h"

extern "C" void ble_init();
extern "C" void ble_deinit();

app::app() {}

void app::init()
{
	m_btn_1.set_cb_on_state_changed([this](button_state_t state) {
		apply_button_function(state, static_cast<button_function_t>(m_config.btn1_func));
	});
	m_btn_2.set_cb_on_state_changed([this](button_state_t state) {
		apply_button_function(state, static_cast<button_function_t>(m_config.btn2_func));
	});
	m_btn_3.set_cb_on_state_changed([this](button_state_t state) {
		apply_button_function(state, static_cast<button_function_t>(m_config.btn3_func));
	});
	m_btn_4.set_cb_on_state_changed([this](button_state_t state) {
		apply_button_function(state, static_cast<button_function_t>(m_config.btn4_func));
	});
	m_btn_cfg.set_cb_on_state_changed([this](button_state_t state) { on_btn_cfg_state_changed(state); });
	m_btn_cfg.set_cb_on_click([this]() { on_btn_cfg_clicked(); });
	m_btn_scroll.set_cb_on_state_changed([this](button_state_t state) { on_btn_scroll_state_changed(state); });
	m_btn_scroll.set_cb_on_click([this]() { on_btn_scroll_clicked(); });

	/* Initialize NVS — it is used to store PHY calibration data and Nimble bonding data */
	esp_err_t ret = nvs_flash_init();
	if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	ESP_ERROR_CHECK(nvs_open(m_nvs_namespace, NVS_READWRITE, &m_nvs_handle));

	// Initialize Bluetooth
	ble_init();

	// Configure SPI bus
	spi_bus_config_t buscfg = {};
	buscfg.miso_io_num		= PIN_NUM_MISO;
	buscfg.mosi_io_num		= PIN_NUM_MOSI;
	buscfg.sclk_io_num		= PIN_NUM_CLK;
	buscfg.quadwp_io_num	= -1;
	buscfg.quadhd_io_num	= -1;
	buscfg.max_transfer_sz	= 32;
	ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

	m_sensor.init(SPI3_HOST, PIN_NUM_CS, PIN_NUM_MOTION, m_config.dpi,
				  [this](int16_t dx, int16_t dy) { sensor_motion_callback(dx, dy); });

	// Initialize I2C bus for the OLED display
	i2c_master_bus_config_t i2c_mst_config		= {};
	i2c_mst_config.clk_source					= I2C_CLK_SRC_DEFAULT;
	i2c_mst_config.i2c_port						= -1; // auto select
	i2c_mst_config.scl_io_num					= PIN_NUM_I2C_SCL;
	i2c_mst_config.sda_io_num					= PIN_NUM_I2C_SDA;
	i2c_mst_config.glitch_ignore_cnt			= 7;
	i2c_mst_config.flags.enable_internal_pullup = true;
	ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &m_h_i2c_bus));

	i2c_device_config_t dev_cfg = {};
	dev_cfg.dev_addr_length		= I2C_ADDR_BIT_LEN_7;
	dev_cfg.device_address		= 0x3C;
	dev_cfg.scl_speed_hz		= 400000; // 400kHz

	ESP_ERROR_CHECK(i2c_master_bus_add_device(m_h_i2c_bus, &dev_cfg, &m_h_i2c_dev));
	m_ui.init(m_h_i2c_dev);

	apply_config();

	m_connection_state_timer.start(1000, true, [this]() { on_update_connection_state(); });

	m_battery.set_callback([this](int voltage, int level) { on_battery_state_changed(voltage, level); });
	ESP_ERROR_CHECK(m_battery.init());
}

void app::deinit()
{
	// Deinitialize Bluetooth
	ble_deinit();

	// Deinitialize NVS
	if(m_nvs_handle != 0)
	{
		nvs_close(m_nvs_handle);
		m_nvs_handle = 0;
	}
	nvs_flash_deinit();

	m_battery.deinit();
	m_connection_state_timer.stop();
}

void app::on_connection_changed()
{
	bool   connected = hid_get_connected();
	int8_t rssi		 = 0;
	bool   rssi_ok	 = hid_get_rssi(&rssi);
	m_ui.set_connection_state(connected, rssi, rssi_ok);
}

void app::apply_config()
{
	m_sensor.set_dpi(m_config.dpi);
	switch(m_config.sensor_mode)
	{
	case SENSOR_MODE_LOW_POWER:
		m_sensor.low_power_mode();
		break;
	case SENSOR_MODE_OFFICE:
		m_sensor.office_mode();
		break;
	case SENSOR_MODE_GAMING:
		m_sensor.gaming_mode();
		break;
	default:
		m_sensor.high_performance_mode();
		break;
	}
}

void app::sensor_motion_callback(int16_t dx, int16_t dy)
{
	bool		   b_send_report = false;
	int8_t		   wheel		 = 0;
	int8_t		   ac_pan		 = 0;
	static int32_t wheel_buffer	 = 0;
	static int32_t ac_pan_buffer = 0;
	if(m_app_state == APP_STATE_SCROLL_HOLD || m_app_state == APP_STATE_SCROLL_LOCK)
	{
		wheel_buffer  += dy;
		ac_pan_buffer += dx;
		if(wheel_buffer > m_config.scroll_sensitivity)
		{
			wheel		  = -1;
			b_send_report = true;
			wheel_buffer  = 0;
		} else if(wheel_buffer < -m_config.scroll_sensitivity)
		{
			wheel		  = 1;
			b_send_report = true;
			wheel_buffer  = 0;
		}
		if(ac_pan_buffer > m_config.scroll_sensitivity)
		{
			ac_pan		  = -1;
			b_send_report = true;
			ac_pan_buffer = 0;
		} else if(ac_pan_buffer < -m_config.scroll_sensitivity)
		{
			ac_pan		  = 1;
			b_send_report = true;
			ac_pan_buffer = 0;
		}
		dx = 0;
		dy = 0;
	} else
	{
		wheel_buffer  = 0;
		ac_pan_buffer = 0;
		b_send_report = true;
	}
	if(b_send_report)
	{
		send_report(-dx, dy, wheel, ac_pan);
	}
}

void app::on_btn_cfg_state_changed(button_state_t state) {}

void app::on_btn_cfg_clicked() {}

void app::on_btn_scroll_state_changed(button_state_t state)
{
	if(state == button_state_t::pressed)
	{
		if(m_app_state == APP_STATE_DEFAULT)
		{
			m_app_state = APP_STATE_SCROLL_HOLD;
			m_ui.set_ui_state(UI_STATE_SCROLL_LOCK);
		}
	} else
	{
		if(m_app_state == APP_STATE_SCROLL_HOLD)
		{
			m_app_state = APP_STATE_DEFAULT;
			m_ui.set_ui_state(UI_STATE_DEFAULT);
		}
	}
}

void app::on_btn_scroll_clicked()
{
	if(m_app_state == APP_STATE_DEFAULT || m_app_state == APP_STATE_SCROLL_HOLD)
	{
		m_locked_buttons = m_buttons;
		if(m_locked_buttons == 0)
		{
			set_app_state(APP_STATE_SCROLL_LOCK);
		} else
		{
			set_app_state(APP_STATE_LOCK_BUTTONS);
		}
	} else
	{
		m_buttons = 0;
		set_app_state(APP_STATE_DEFAULT);
		send_report();
	}
}

void app::on_update_connection_state()
{
	bool   connected = hid_get_connected();
	int8_t rssi		 = 0;
	bool   rssi_ok	 = hid_get_rssi(&rssi);
	m_ui.set_connection_state(connected, rssi, rssi_ok);
}

void app::on_battery_state_changed(int voltage, int level)
{
	m_ui.set_battery_level(voltage, level);
	if(hid_battery_level_get() != (int) level)
	{
		hid_battery_level_set(static_cast<uint8_t>(level));
	}
}

void app::apply_button_function(button_state_t state, button_function_t func)
{
	if(state == button_state_t::pressed)
	{
		switch(func)
		{
		case BTN_FNC_LEFT:
			m_buttons |= 0x1;
			break;
		case BTN_FNC_RIGHT:
			m_buttons |= 0x2;
			break;
		case BTN_FNC_MIDDLE:
			m_buttons |= 0x4;
			break;
		default:
			break;
		}
	} else
	{
		switch(func)
		{
		case BTN_FNC_LEFT:
			m_buttons &= ~0x1;
			break;
		case BTN_FNC_RIGHT:
			m_buttons &= ~0x2;
			break;
		case BTN_FNC_MIDDLE:
			m_buttons &= ~0x4;
			break;
		default:
			break;
		}
	}
	if(m_app_state != APP_STATE_LOCK_BUTTONS)
	{
		send_report();
	}
}

void app::set_app_state(app_state_t state)
{
	if(m_app_state == state)
		return;
	m_app_state = state;
	switch(m_app_state)
	{
	case APP_STATE_DEFAULT:
		m_ui.set_ui_state(UI_STATE_DEFAULT);
		break;

	case APP_STATE_SCROLL_HOLD:
		m_ui.set_ui_state(UI_STATE_SCROLL_LOCK);
		break;

	case APP_STATE_SCROLL_LOCK:
		m_ui.set_ui_state(UI_STATE_SCROLL_LOCK);
		break;

	case APP_STATE_LOCK_BUTTONS:
		m_ui.set_locked_buttons(m_locked_buttons);
		m_ui.set_ui_state(UI_STATE_LOCK_BUTTONS);
		break;

	default:
		break;
	}
}

void app::send_report(int16_t dx, int16_t dy, int8_t wheel, int8_t ac_pan)
{
	hid_mouse_send_report(get_report_buttons(), dx, dy, wheel, ac_pan);
}
