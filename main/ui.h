#ifndef _UI_H
#define _UI_H

#include <vector>
#include <string>
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "oled/ssd1306.h"
#include "types.h"
#include "menu.h"

enum ui_state_t
{
    UI_STATE_DEFAULT,
    UI_STATE_SCROLL_LOCK,
    UI_STATE_LOCK_BUTTONS,
    UI_STATE_GO_SLEEP,
    UI_STATE_MENU,
};

class trackball_ui
{
  private:
    ssd1306_t m_oled_data = {};
    // connection state
    bool       m_connected      = false;
    bool       m_rssi_valid     = false;
    int8_t     m_rssi           = 0;
    uint8_t    m_locked_buttons = 0;
    ui_state_t m_ui_state       = UI_STATE_DEFAULT;
    int        m_bat_mV         = 0;
    int        m_bat_level      = 0;
    int        m_dpi            = 600;
    uint8_t    m_scroll_mode    = SCROLL_MODE_HIGH_RES | SCROLL_MODE_ENABLE_HSCROLL | SCROLL_MODE_ENABLE_VSCROLL;

    ui_menu::submenu* m_current_menu = nullptr;

  public:
    trackball_ui() = default;
    ~trackball_ui();

    void init(i2c_master_dev_handle_t oled_dev);
    void deinit();
    void power_off();
    void power_on();

    void set_connection_state(bool connected, int8_t rssi, bool rssi_valid);
    void set_ui_state(ui_state_t state);
    void set_locked_buttons(uint8_t buttons)
    {
        m_locked_buttons = buttons;
    }
    void set_battery_level(int bat_mV, int level);
    void set_scroll_mode(uint8_t scroll_mode)
    {
        m_scroll_mode = scroll_mode;
        if(m_ui_state == UI_STATE_DEFAULT)
        {
            draw_ui_default();
            ssd1306_show(&m_oled_data);
        } else if(m_ui_state == UI_STATE_SCROLL_LOCK)
        {
            draw_ui_scroll_lock();
            ssd1306_show(&m_oled_data);
        }
    }
    void set_dpi(int dpi)
    {
        m_dpi = dpi;
        if(m_ui_state == UI_STATE_DEFAULT)
        {
            draw_ui_default();
            ssd1306_show(&m_oled_data);
        }
    }

    void start_menu(ui_menu::submenu* menu);
    void on_scroll(int16_t wheel);
    void on_menu_confirm();
    bool on_menu_back();

  private:
    void draw_status_line();
    void draw_ui_state();
    void draw_ui_default();
    void draw_ui_scroll_lock();
    void draw_ui_lock_buttons();
    void draw_ui_go_sleep();
    void draw_ui_menu();
};

#endif // _UI_H
