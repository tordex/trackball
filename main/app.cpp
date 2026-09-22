#include "freertos/FreeRTOS.h"
#include "app.h"
#include "esp_log.h"
#include "hid_func.h"
#include "nvs_flash.h"
#include "driver/i2c_master.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "pins.h"
#include <algorithm>

extern "C" void ble_init();
extern "C" void ble_deinit();
extern "C" int  ble_forget_bonds();

app::app() {}

void app::init()
{
    hid_init();

    m_events_queue = xQueueCreate(32, sizeof(app_event_data_t));

    /* Initialize NVS — it is used to store PHY calibration data and Nimble bonding data */
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(nvs_open(m_nvs_namespace, NVS_READWRITE, &m_nvs_handle));
    load_config_from_nvs();

    // Initialize Bluetooth
    ble_init();

    // Configure SPI bus
    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num      = PIN_NUM_MISO;
    buscfg.mosi_io_num      = PIN_NUM_MOSI;
    buscfg.sclk_io_num      = PIN_NUM_CLK;
    buscfg.quadwp_io_num    = -1;
    buscfg.quadhd_io_num    = -1;
    buscfg.max_transfer_sz  = 32;
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // Initialize I2C bus and OLED early so startup errors can be shown without serial logs.
    i2c_master_bus_config_t i2c_mst_config      = {};
    i2c_mst_config.clk_source                   = I2C_CLK_SRC_DEFAULT;
    i2c_mst_config.i2c_port                     = -1; // auto select
    i2c_mst_config.scl_io_num                   = PIN_NUM_I2C_SCL;
    i2c_mst_config.sda_io_num                   = PIN_NUM_I2C_SDA;
    i2c_mst_config.glitch_ignore_cnt            = 7;
    i2c_mst_config.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &m_h_i2c_bus));

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length     = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address      = 0x3C;
    dev_cfg.scl_speed_hz        = 400000; // 400kHz

    ESP_ERROR_CHECK(i2c_master_bus_add_device(m_h_i2c_bus, &dev_cfg, &m_h_i2c_dev));
    m_ui.init(m_h_i2c_dev);
    m_ui.power_on();
    m_ui.set_dpi(m_config.dpi);
    uint8_t scroll_mode = m_config.scroll_mode;
    if(m_config.enable_high_res_scroll)
    {
        scroll_mode |= SCROLL_MODE_HIGH_RES;
    }
    m_ui.set_scroll_mode(scroll_mode);

    esp_err_t sensor_ret = m_sensor.init(SPI3_HOST, PIN_NUM_CS, PIN_NUM_MOTION,
                                         [this](int16_t dx, int16_t dy) { sensor_motion_callback(dx, dy); });
    if(sensor_ret != ESP_OK)
    {
        ESP_LOGE("APP", "PAW3395 init failed: %d", sensor_ret);
        abort();
    }

    apply_config();

    if(m_config.deep_sleep_timeout_ms > 0)
    {
        m_suspend_timer.start(m_config.deep_sleep_timeout_ms, false, [this]() { send_event(app_event_sleep); });
    }

    m_connection_state_timer.start(1000, true, [this]() { send_event(app_event_update_connection_state); });
    oled_timer_start();

    m_battery.set_callback([this](int voltage, int level) { send_event(app_event_battery_state_changed); });
    ESP_ERROR_CHECK(m_battery.init());

    m_btn_1      = new button(PIN_BTN1);       // Button 1 (left-top)
    m_btn_2      = new button(PIN_BTN2);       // Button 2	(left-bottom)
    m_btn_3      = new button(PIN_BTN3);       // Button 3 (right-top)
    m_btn_mode   = new button(PIN_BTN_MODE);   // Button 4 (right-bottom)
    m_btn_scroll = new button(PIN_BTN_SCROLL); // Scroll button
    m_btn_cfg    = new button(PIN_BTN_CFG);    // Configuration button

    // Configure button callbacks
    m_btn_1->set_cb_on_state_changed([this](button_state_t state) {
        on_activity_detected();
        apply_button_function(state, static_cast<button_function_t>(m_config.btn1_func));
    });
    m_btn_2->set_cb_on_state_changed([this](button_state_t state) {
        on_activity_detected();
        apply_button_function(state, static_cast<button_function_t>(m_config.btn2_func));
    });
    m_btn_3->set_cb_on_state_changed([this](button_state_t state) {
        on_activity_detected();
        apply_button_function(state, static_cast<button_function_t>(m_config.btn3_func));
    });
    m_btn_mode->set_cb_on_click([this]() { send_event(app_event_btn_mode_clicked); });
    m_btn_mode->set_cb_on_hold_down([this]() { send_event(app_event_btn_mode_hold_down); });
    m_btn_cfg->set_cb_on_click([this]() { send_event(app_event_btn_cfg_clicked); });
    m_btn_cfg->set_cb_on_hold_down([this]() { send_event(app_event_forget_bonds); });
    m_btn_scroll->set_cb_on_state_changed(
        [this](button_state_t state) { send_event(app_event_btn_scroll_state_changed, static_cast<uint32_t>(state)); });
    m_btn_scroll->set_cb_on_click([this]() { send_event(app_event_btn_scroll_clicked); });
}

esp_err_t app::save_config_to_nvs()
{
    if(m_nvs_handle == 0)
    {
        ESP_LOGW("APP", "NVS handle is not initialized, skipping config save");
        return ESP_ERR_INVALID_STATE;
    }

    persisted_app_config_t persisted{};
    persisted.config = m_config;

    esp_err_t ret = nvs_set_blob(m_nvs_handle, m_nvs_cfg_key, &persisted, sizeof(persisted));
    if(ret != ESP_OK)
    {
        ESP_LOGE("APP", "Failed to write config to NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_commit(m_nvs_handle);
    if(ret != ESP_OK)
    {
        ESP_LOGE("APP", "Failed to commit config to NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

bool app::load_config_from_nvs()
{
    if(m_nvs_handle == 0)
    {
        ESP_LOGW("APP", "NVS handle is not initialized, using default config");
        return false;
    }

    persisted_app_config_t persisted{};
    size_t                 size = sizeof(persisted);
    esp_err_t              ret  = nvs_get_blob(m_nvs_handle, m_nvs_cfg_key, &persisted, &size);
    if(ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI("APP", "Config not found in NVS, using defaults");
        return false;
    }

    if(ret != ESP_OK)
    {
        ESP_LOGW("APP", "Failed to read config from NVS (%s), using defaults", esp_err_to_name(ret));
        return false;
    }

    if(size != sizeof(persisted))
    {
        ESP_LOGW("APP", "Stored config size mismatch (%u), using defaults", (unsigned int) size);
        return false;
    }

    if(persisted.version != k_nvs_storage_version)
    {
        ESP_LOGW("APP", "Stored config version mismatch (%u), using defaults", persisted.version);
        return false;
    }

    m_config = persisted.config;
    return true;
}

void app::deinit()
{
    m_sensor.stop_motion_task();

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
    m_suspend_timer.stop();
    m_ui.power_off();
}

void app::on_connection_changed()
{
    bool   connected = hid_get_connected();
    int8_t rssi      = 0;
    bool   rssi_ok   = hid_get_rssi(&rssi);
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

void app::on_activity_detected()
{
    if(m_config.deep_sleep_timeout_ms > 0)
    {
        m_suspend_timer.reset();
    }
}

void app::configure_deep_sleep_wakeup_sources()
{
    m_connection_state_timer.stop();

    const gpio_num_t wake_pins[] = {PIN_NUM_MOTION};

    uint64_t wake_pin_mask = 0;
    for(gpio_num_t pin : wake_pins)
    {
        if(!rtc_gpio_is_valid_gpio(pin))
        {
            ESP_LOGW("APP", "GPIO%d is not RTC-capable, skipping deep sleep wake setup", pin);
            continue;
        }

        ESP_ERROR_CHECK(rtc_gpio_init(pin));
        ESP_ERROR_CHECK(rtc_gpio_pullup_en(pin));
        ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(pin));
        wake_pin_mask |= 1ULL << pin;
    }

    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON));
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(wake_pin_mask, ESP_EXT1_WAKEUP_ANY_LOW));
}

void app::enter_deep_sleep()
{
    if(m_config.deep_sleep_timeout_ms == 0)
    {
        return;
    }
    deinit();

    m_btn_1->enter_deep_sleep();
    m_btn_2->enter_deep_sleep();
    m_btn_3->enter_deep_sleep();
    m_btn_mode->enter_deep_sleep();
    m_btn_scroll->enter_deep_sleep();
    m_btn_cfg->enter_deep_sleep();

    configure_deep_sleep_wakeup_sources();
    ESP_LOGI("APP", "Entering deep sleep");
    esp_deep_sleep_start();
}

extern uint8_t resolution_multiplier;

void app::sensor_motion_callback(int16_t dx, int16_t dy)
{
    on_activity_detected();

    bool           b_send_report = false;
    int8_t         wheel         = 0;
    int8_t         ac_pan        = 0;
    static int32_t wheel_buffer  = 0;
    static int32_t ac_pan_buffer = 0;
    if(m_app_state == APP_STATE_SCROLL_HOLD || m_app_state == APP_STATE_SCROLL_LOCK)
    {
        if(m_config.enable_high_res_scroll)
        {
            if(m_config.scroll_mode & SCROLL_MODE_ENABLE_VSCROLL)
            {
                wheel = std::clamp(dy * 2, -32768, 32767);
            }
            if(m_config.scroll_mode & SCROLL_MODE_ENABLE_HSCROLL)
            {
                ac_pan = std::clamp(dx * 2, -32768, 32767);
            }
            b_send_report = wheel != 0 || ac_pan != 0;
        } else
        {
            if(m_config.scroll_mode & SCROLL_MODE_ENABLE_VSCROLL)
            {
                wheel_buffer += dy;
            }
            if(m_config.scroll_mode & SCROLL_MODE_ENABLE_HSCROLL)
            {
                ac_pan_buffer += dx;
            }
            if(wheel_buffer > m_config.scroll_sensitivity)
            {
                wheel         = (int16_t) resolution_multiplier;
                b_send_report = true;
                wheel_buffer  = 0;
            } else if(wheel_buffer < -m_config.scroll_sensitivity)
            {
                wheel         = -((int16_t) resolution_multiplier);
                b_send_report = true;
                wheel_buffer  = 0;
            }
            if(ac_pan_buffer > m_config.scroll_sensitivity)
            {
                ac_pan        = (int16_t) resolution_multiplier;
                b_send_report = true;
                ac_pan_buffer = 0;
            } else if(ac_pan_buffer < -m_config.scroll_sensitivity)
            {
                ac_pan        = -((int16_t) resolution_multiplier);
                b_send_report = true;
                ac_pan_buffer = 0;
            }
        }
        dx = 0;
        dy = 0;
    } else
    {
        wheel_buffer  = 0;
        ac_pan_buffer = 0;
        b_send_report = dx != 0 || dy != 0;
    }
    if(b_send_report)
    {
        send_report(-dx, dy, wheel, ac_pan);
        vTaskDelay(pdMS_TO_TICKS(2));
    } else
    {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void app::on_btn_cfg_clicked()
{
    on_activity_detected();
    oled_timer_reset();
    int dpi_idx = 1;
    for(int i = 0; i < PREDEFINED_DPI_COUNT; i++)
    {
        if(m_config.dpi == m_config.predefined_dpi[i])
        {
            dpi_idx = i;
            break;
        }
    }
    dpi_idx      = (dpi_idx + 1) % PREDEFINED_DPI_COUNT;
    m_config.dpi = m_config.predefined_dpi[dpi_idx];
    save_config_to_nvs();
    m_sensor.set_dpi(m_config.dpi);
    m_ui.set_dpi(m_config.dpi);
}

void app::on_forget_bonds()
{
    on_activity_detected();
    oled_timer_reset();
    ble_forget_bonds();
}

void app::on_btn_mode_clicked()
{
    on_activity_detected();
    if(m_app_state == APP_STATE_DEFAULT)
    {
        oled_timer_reset();
    }
    const int mode_count        = 3;
    uint8_t   modes[mode_count] = {
        SCROLL_MODE_ENABLE_HSCROLL | SCROLL_MODE_ENABLE_VSCROLL,
        SCROLL_MODE_ENABLE_VSCROLL,
        SCROLL_MODE_ENABLE_HSCROLL,
    };

    int mode = 0;
    for(int i = 0; i < mode_count; i++)
    {
        if(m_config.scroll_mode == modes[i])
        {
            mode = i;
            break;
        }
    }
    mode                 = (mode + 1) % mode_count;
    m_config.scroll_mode = modes[mode];
    save_config_to_nvs();
    uint8_t scroll_mode = m_config.scroll_mode;
    if(m_config.enable_high_res_scroll)
    {
        scroll_mode |= SCROLL_MODE_HIGH_RES;
    }
    m_ui.set_scroll_mode(scroll_mode);
}

void app::on_btn_mode_hold_down()
{
    on_activity_detected();
    if(m_app_state == APP_STATE_DEFAULT)
    {
        oled_timer_reset();
    }
    m_config.enable_high_res_scroll = !m_config.enable_high_res_scroll;
    save_config_to_nvs();

    uint8_t scroll_mode = m_config.scroll_mode;
    if(m_config.enable_high_res_scroll)
    {
        scroll_mode |= SCROLL_MODE_HIGH_RES;
    }
    m_ui.set_scroll_mode(scroll_mode);
}

void app::on_btn_scroll_state_changed(button_state_t state)
{
    on_activity_detected();

    if(state == button_state_t::pressed)
    {
        if(m_app_state == APP_STATE_DEFAULT)
        {
            oled_timer_stop();
            m_app_state = APP_STATE_SCROLL_HOLD;
            m_ui.set_ui_state(UI_STATE_SCROLL_LOCK);
            m_sensor.set_dpi(m_config.scroll_dpi);
        }
    } else
    {
        if(m_app_state == APP_STATE_SCROLL_HOLD)
        {
            oled_timer_reset();
            m_app_state = APP_STATE_DEFAULT;
            m_ui.set_ui_state(UI_STATE_DEFAULT);
            m_sensor.set_dpi(m_config.dpi);
        }
    }
}

void app::on_btn_scroll_clicked()
{
    on_activity_detected();
    if(m_app_state == APP_STATE_DEFAULT || m_app_state == APP_STATE_SCROLL_HOLD)
    {
        m_locked_buttons = m_buttons;
        if(m_locked_buttons == 0)
        {
            set_app_state(APP_STATE_SCROLL_LOCK);
            m_sensor.set_dpi(m_config.scroll_dpi);
        } else
        {
            set_app_state(APP_STATE_LOCK_BUTTONS);
        }
        oled_timer_stop();
    } else
    {
        m_buttons = 0;
        oled_timer_reset();
        set_app_state(APP_STATE_DEFAULT);
        m_sensor.set_dpi(m_config.dpi);
        send_report();
    }
}

void app::on_update_connection_state()
{
    bool   connected = hid_get_connected();
    int8_t rssi      = 0;
    bool   rssi_ok   = hid_get_rssi(&rssi);
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
    on_activity_detected();

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
    {
        return;
    }
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

void app::send_report(int16_t dx, int16_t dy, int16_t wheel, int16_t ac_pan)
{
    hid_mouse_send_report(get_report_buttons(), dx, dy, wheel, ac_pan);
}

void app::loop()
{
    app_event_data_t event_data;
    while(true)
    {
        if(m_events_queue == nullptr)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if(xQueueReceive(m_events_queue, &event_data, portMAX_DELAY))
        {
            switch(event_data.event)
            {
            case app_event_update_connection_state:
                on_update_connection_state();
                break;

            case app_event_sleep:
                enter_deep_sleep();
                break;

            case app_event_battery_state_changed:
                {
                    int voltage, level;
                    m_battery.get_state(voltage, level);
                    on_battery_state_changed(voltage, level);
                }
                break;

            case app_event_btn_cfg_clicked:
                on_btn_cfg_clicked();
                break;

            case app_event_forget_bonds:
                on_forget_bonds();
                break;

            case app_event_btn_mode_clicked:
                on_btn_mode_clicked();
                break;

            case app_event_btn_mode_hold_down:
                on_btn_mode_hold_down();
                break;

            case app_event_btn_scroll_clicked:
                on_btn_scroll_clicked();
                break;

            case app_event_btn_scroll_state_changed:
                on_btn_scroll_state_changed(static_cast<button_state_t>(event_data.data));
                break;

            case app_event_oled_power_off:
                m_ui.power_off();
                break;

            case app_event_oled_power_on:
                m_ui.power_on();
                break;

            default:
                break;
            }
        }
    }
}
