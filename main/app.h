#pragma once

#include "battery.h"
#include "paw3395.h"
#include "button.h"
#include "ui.h"
#include "pins.h"
#include "timer.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "types.h"

enum app_state_t
{
    APP_STATE_DEFAULT,      // Default state. Normal operation
    APP_STATE_SCROLL_HOLD,  // Scroll button is held. Send scroll events on motion.
    APP_STATE_SCROLL_LOCK,  // Scroll lock active (after scroll button click). Send scroll events on motion.
    APP_STATE_LOCK_BUTTONS, // Pressed buttons are locked (after config button click)
};

enum button_function_t
{
    BTN_FNC_NONE,
    BTN_FNC_LEFT,
    BTN_FNC_RIGHT,
    BTN_FNC_MIDDLE,
};

enum sensor_mode_t : uint8_t
{
    SENSOR_MODE_HIGH_PERFORMANCE,
    SENSOR_MODE_LOW_POWER,
    SENSOR_MODE_OFFICE,
    SENSOR_MODE_GAMING,
};

const int PREDEFINED_DPI_COUNT = 4;
struct app_config
{
    uint8_t       btn1_func                            = BTN_FNC_LEFT;
    uint8_t       btn2_func                            = BTN_FNC_RIGHT;
    uint8_t       btn3_func                            = BTN_FNC_MIDDLE;
    uint8_t       scroll_sensitivity                   = 100;
    uint32_t      deep_sleep_timeout_ms                = 60000; // 1 minute
    uint32_t      oled_timeout_ms                      = 0;
    uint16_t      dpi                                  = 600;
    uint16_t      scroll_dpi                           = 600;
    sensor_mode_t sensor_mode                          = SENSOR_MODE_HIGH_PERFORMANCE;
    uint8_t       scroll_mode                          = SCROLL_MODE_ENABLE_HSCROLL | SCROLL_MODE_ENABLE_VSCROLL;
    bool          enable_high_res_scroll               = true;
    uint16_t      predefined_dpi[PREDEFINED_DPI_COUNT] = {200, 600, 1200, 2000};
};

enum app_event_t
{
    app_event_update_connection_state,
    app_event_battery_state_changed,
    app_event_sleep,
    app_event_btn_cfg_clicked,
    app_event_btn_mode_clicked,
    app_event_btn_mode_hold_down,
    app_event_btn_scroll_clicked,
    app_event_btn_scroll_state_changed,
    app_event_oled_power_off,
    app_event_oled_power_on,
};

struct app_event_data_t
{
    app_event_t event;
    uint32_t    data;
};

class app
{
  private:
    battery      m_battery;
    paw3395      m_sensor;
    button*      m_btn_1      = nullptr; // Button 1 (left-top)
    button*      m_btn_2      = nullptr; // Button 2	(left-bottom)
    button*      m_btn_3      = nullptr; // Button 3 (right-top)
    button*      m_btn_mode   = nullptr; // Button 4 (right-bottom)
    button*      m_btn_scroll = nullptr; // Scroll button
    button*      m_btn_cfg    = nullptr; // Configuration button
    trackball_ui m_ui;
    app_config   m_config;
    app_state_t  m_app_state = APP_STATE_DEFAULT;

    uint8_t m_buttons        = 0;
    uint8_t m_locked_buttons = 0;

    // for nvs_storage
    const char*  m_nvs_namespace = "storage";
    const char*  m_nvs_cfg_key   = "app_cfg";
    nvs_handle_t m_nvs_handle    = 0;

    i2c_master_bus_handle_t m_h_i2c_bus = nullptr;
    i2c_master_dev_handle_t m_h_i2c_dev = nullptr;

    QueueHandle_t m_events_queue = nullptr;

    timer m_connection_state_timer{"connection_state"};
    timer m_suspend_timer{"suspend"};
    timer m_oled_timer{"oled"};

  public:
    app();
    ~app() = default;

    void init();
    void deinit();
    void on_connection_changed();
    void loop();
    void send_event(app_event_t event, uint32_t data = 0)
    {
        if(m_events_queue)
        {
            app_event_data_t event_data{event, data};
            xQueueSend(m_events_queue, &event_data, 0);
        }
    }

  private:
    constexpr static uint32_t k_nvs_storage_version = 2;

    struct persisted_app_config_t
    {
        uint32_t   version = k_nvs_storage_version;
        app_config config{};
    };

    esp_err_t save_config_to_nvs();
    bool      load_config_from_nvs();

    void apply_config();
    void on_activity_detected();
    void enter_deep_sleep();
    void configure_deep_sleep_wakeup_sources();
    void sensor_motion_callback(int16_t dx, int16_t dy);
    void on_btn_cfg_clicked();
    void on_btn_mode_clicked();
    void on_btn_mode_hold_down();
    void on_btn_scroll_state_changed(button_state_t state);
    void on_btn_scroll_clicked();
    void on_update_connection_state();
    void on_battery_state_changed(int voltage, int level);
    void apply_button_function(button_state_t state, button_function_t func);

    void set_app_state(app_state_t state);
    void send_report(int16_t dx = 0, int16_t dy = 0, int16_t wheel = 0, int16_t ac_pan = 0);

    uint8_t get_report_buttons() const
    {
        return m_app_state == APP_STATE_LOCK_BUTTONS ? m_locked_buttons : m_buttons;
    }

    void oled_timer_stop()
    {
        if(m_config.oled_timeout_ms > 0)
        {
            m_oled_timer.stop();
            m_ui.power_on();
        }
    }

    void oled_timer_start()
    {
        if(m_config.oled_timeout_ms > 0)
        {
            m_oled_timer.start(m_config.oled_timeout_ms, false, [this]() { send_event(app_event_oled_power_off); });
            m_ui.power_on();
        }
    }

    void oled_timer_reset()
    {
        if(m_config.oled_timeout_ms > 0)
        {
            m_oled_timer.reset();
            m_ui.power_on();
        }
    }
};
