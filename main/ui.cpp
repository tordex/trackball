#include "ui.h"

// Include bitmap definitions
#include "images/images.c"


trackball_ui::~trackball_ui()
{
    deinit();
}

void trackball_ui::init(i2c_master_dev_handle_t oled_dev)
{
    ssd1306_init(&m_oled_data, 128, 64, oled_dev);
    ssd1306_clear(&m_oled_data);
    ssd1306_show(&m_oled_data);
    draw_status_line();
    draw_ui_state();
    ssd1306_show(&m_oled_data);
}

void trackball_ui::deinit()
{
    ssd1306_deinit(&m_oled_data);
}

void trackball_ui::set_connection_state(bool connected, int8_t rssi, bool rssi_valid)
{
    if(m_connected == connected && m_rssi == rssi && m_rssi_valid == rssi_valid)
        return;

    m_connected = connected;
    m_rssi = rssi;
    m_rssi_valid = rssi_valid;
    draw_status_line();
    ssd1306_show(&m_oled_data);
}

void trackball_ui::set_ui_state(ui_state_t state)
{
    if(m_ui_state != state)
    {
        m_ui_state = state;
        draw_ui_state();
    }
}

void trackball_ui::set_battery_level(int bat_mV, int level)
{
    bool show = false;
    if(m_bat_mV != bat_mV)
    {
        m_bat_mV = bat_mV;
        if(m_ui_state == UI_STATE_DEFAULT)
        {
            draw_ui_default();
            show = true;
        }
    }
    if(m_bat_level != level)
    {
        m_bat_level = level;
        if(m_ui_state == UI_STATE_DEFAULT)
        {
            draw_status_line();
            show = true;
        }
    }
    if(show)
    {
        ssd1306_show(&m_oled_data);
    }
}

void trackball_ui::draw_status_line()
{
    ssd1306_clear_square(&m_oled_data, 0, 0, 128, 15);
    if(m_connected)
    {
        if(m_rssi_valid)
        {
            if(m_rssi > -55)
            {
                ssd1306_bmp_show_image_with_offset(&m_oled_data, signal_4of4_bmp, signal_4of4_bmp_len, 0, 0);
            } else if(m_rssi > -65)
            {
                ssd1306_bmp_show_image_with_offset(&m_oled_data, signal_3of4_bmp, signal_3of4_bmp_len, 0, 0);
            } else if(m_rssi > -70)
            {
                ssd1306_bmp_show_image_with_offset(&m_oled_data, signal_2of4_bmp, signal_2of4_bmp_len, 0, 0);
            } else
            {
                ssd1306_bmp_show_image_with_offset(&m_oled_data, signal_1of4_bmp, signal_1of4_bmp_len, 0, 0);
            }
            // Draw RSSI if valid
            char level_str[20] = {};
            snprintf(level_str, sizeof(level_str), "%ddBm", m_rssi);
            ssd1306_draw_string(&m_oled_data, 17, 2, 1, level_str);
        } else
        {
            ssd1306_bmp_show_image_with_offset(&m_oled_data, signal_disconnected_bmp, signal_disconnected_bmp_len, 0, 0);
        }
    } else
    {
            ssd1306_bmp_show_image_with_offset(&m_oled_data, signal_disconnected_bmp, signal_disconnected_bmp_len, 0, 0);
    }

    struct
    {
        unsigned char* bmp;
        size_t len;
    } levels[] = {
        { battery_0of6_bmp, battery_0of6_bmp_len },
        { battery_1of6_bmp, battery_1of6_bmp_len },
        { battery_2of6_bmp, battery_2of6_bmp_len },
        { battery_3of6_bmp, battery_3of6_bmp_len },
        { battery_4of6_bmp, battery_4of6_bmp_len },
        { battery_5of6_bmp, battery_5of6_bmp_len },
        { battery_6of6_bmp, battery_6of6_bmp_len },
    };
    float idx_f = (float) m_bat_level * 7.0f / 100.0f;
    int idx = (int)(idx_f);
    if(idx_f - idx >= 0.5f) idx++;
    if(idx < 0) idx = 0;
    if(idx > 6) idx = 6;
    ssd1306_bmp_show_image_with_offset(&m_oled_data, levels[idx].bmp, levels[idx].len, 111, 0);

    // Draw battery percentage
    char bat_str[10] = {};
    snprintf(bat_str, sizeof(bat_str), " %3d%%", m_bat_level);
    ssd1306_draw_string(&m_oled_data, 80, 2, 1, bat_str);

    // Draw separator line
    ssd1306_draw_line(&m_oled_data, 0, 13, 128, 13);
}

void trackball_ui::draw_ui_state()
{
    switch(m_ui_state)
    {
        case UI_STATE_DEFAULT:
            draw_ui_default();
            break;
        case UI_STATE_SCROLL_LOCK:
            draw_ui_scroll_lock();
            break;
        case UI_STATE_LOCK_BUTTONS:
            draw_ui_lock_buttons();
            break;
        default:
            break;
    }
    ssd1306_show(&m_oled_data);
}

void trackball_ui::draw_ui_default()
{
    ssd1306_clear_square(&m_oled_data, 0, 16, 128, 48);
    // TODO: implement
    char battery_str[30] = {};
    snprintf(battery_str, sizeof(battery_str), "Battery: %.3fV", m_bat_mV / 1000.0f);
    ssd1306_draw_string(&m_oled_data, 0, 16, 1, battery_str);
}

void trackball_ui::draw_ui_scroll_lock()
{
    ssd1306_clear_square(&m_oled_data, 0, 16, 128, 48);
    ssd1306_bmp_show_image_with_offset(&m_oled_data, scroll_lock_bmp, scroll_lock_bmp_len, 46, 16);
}

void trackball_ui::draw_ui_lock_buttons()
{
    ssd1306_clear_square(&m_oled_data, 0, 16, 128, 48);
    ssd1306_bmp_show_image_with_offset(&m_oled_data, lock_buttons_bmp, lock_buttons_bmp_len, 92, 16);
    int idx = 0;
    const char* strs[3];
    if(m_locked_buttons & 0x01)
    {
        strs[idx++] = "LEFT";
    }
    if(m_locked_buttons & 0x02)
    {
        strs[idx++] = "RIGHT";
    }
    if(m_locked_buttons & 0x04)
    {
        strs[idx++] = "MIDDLE";
    }
    int top = 16 + 48 / 2 - idx * 16 / 2;
    for(int i = 0; i < idx; i++)
    {
        ssd1306_draw_string(&m_oled_data, 0, top + i * 16, 2, strs[i]);
    }
}
