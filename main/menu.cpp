#include "menu.h"
#include "oled/ssd1306.h"
#include <cstdarg>

ui_menu::confirm_result ui_menu::submenu::on_confirm(bool is_focused)
{
    if(is_focused)
    {
        if(m_focused_index >= 0 && m_focused_index < m_items.size())
        {
            if(std::visit([](auto&& arg) { return arg.on_confirm(true); }, m_items[m_focused_index]) == CONFIRM_GO_BACK)
            {
                m_focused_index = -1;
            }
            return CONFIRM_PROCESSES;
        }
        if(m_current_index >= 0 && m_current_index < m_items.size())
        {
            auto res = std::visit([](auto&& arg) { return arg.on_confirm(false); }, m_items[m_current_index]);
            if(res == CONFIRM_GRAB_FOCUS)
            {
                m_focused_index = m_current_index;
                return CONFIRM_PROCESSES; // Not focused, so return false
            }
            return res;
        }
    }
    // Return true to grab focus for this submenu
    return CONFIRM_GRAB_FOCUS;
}

bool ui_menu::submenu::on_back()
{
    if(m_focused_index >= 0 && m_focused_index < m_items.size())
    {
        if(std::visit([](auto&& arg) { return arg.on_back(); }, m_items[m_focused_index]))
        {
            m_focused_index = -1;
            return false;
        }
    }
    return true;
}

bool ui_menu::submenu::on_scroll(int step)
{
    if(m_focused_index >= 0 && m_focused_index < m_items.size())
    {
        return std::visit([step](auto&& arg) { return arg.on_scroll(step); }, m_items[m_focused_index]);
    }
    auto new_idx = clamp_menu_value(m_current_index + step);
    if(new_idx != m_current_index)
    {
        m_current_index = new_idx;
        update_menu_top_idx();
        return true;
    }
    return false;
}

void ui_menu::submenu::on_draw_focused(ssd1306_t* p, int x, int y)
{
    if(m_focused_index >= 0 && m_focused_index < m_items.size())
    {
        std::visit([p, x, y](auto&& arg) { arg.on_draw_focused(p, x, y); }, m_items[m_focused_index]);
        return;
    }
    ssd1306_clear(p);
    for(int i = m_top_index; i < m_items.size() && i <= m_top_index + ITEMS_ON_SCREEN - 1; i++)
    {
        if(i == m_current_index)
        {
            ssd1306_draw_string(p, x, y, 2, ">");
        }
        std::visit([p, x, y](auto&& arg) { arg.on_draw(p, x + 10, y); }, m_items[i]);
        y += 16;
    }
}

void ui_menu::submenu::on_draw(ssd1306_t* p, int x, int y)
{
    std::string text;
    if(m_cb_get_text)
    {
        text = m_cb_get_text();
    } else
    {
        text = m_name;
    }
    ssd1306_draw_string(p, x, y, 2, text.c_str());
}

int ui_menu::submenu::clamp_menu_value(int new_index)
{
    if(new_index < 0)
    {
        return 0;
    } else if(new_index > m_items.size() - 1)
    {
        return m_items.size() - 1;
    }
    return new_index;
}

bool ui_menu::submenu::update_menu_top_idx()
{
    if(m_current_index < m_top_index)
    {
        m_top_index = m_current_index;
        return true;
    } else if(m_current_index >= m_top_index + ITEMS_ON_SCREEN)
    {
        m_top_index = m_current_index - ITEMS_ON_SCREEN + 1;
        return true;
    }
    return false;
}

void ui_menu::menu_item::on_draw(ssd1306_t* p, int x, int y)
{
    std::string text;
    if(m_cb_get_text)
    {
        text = m_cb_get_text();
    } else
    {
        text = m_name;
    }
    if(m_cb_get_marker)
    {
        text += " " + m_cb_get_marker();
    }
    ssd1306_draw_string(p, x, y, 2, text.c_str());
}

ui_menu::confirm_result ui_menu::value_editor::on_confirm(bool is_focused)
{
    if(is_focused)
    {
        if(m_cb_confirm)
        {
            m_cb_confirm(m_current_value);
            m_value = m_current_value;
        }
        return CONFIRM_GO_BACK;
    }
    m_current_value = m_value;
    return CONFIRM_GRAB_FOCUS;
}

bool ui_menu::value_editor::on_scroll(int step)
{
    auto new_val    = std::clamp(m_current_value + step * m_step, m_min, m_max);
    auto changed    = (new_val != m_current_value);
    m_current_value = new_val;
    return changed;
}

void ui_menu::value_editor::on_draw_focused(ssd1306_t* p, int x, int y)
{
    ssd1306_clear(p);
    ssd1306_draw_string(p, x, y, 2, m_name.data());
    y += 16;
    ssd1306_draw_string(p, x, y, 2, m_sub_title.data());
    y += 16;
    ssd1306_draw_string(p, x, y, 2, ("->" + std::to_string(m_current_value)).c_str());
}

void ui_menu::value_editor::on_draw(ssd1306_t* p, int x, int y)
{
    std::string text;
    if(m_cb_get_text)
    {
        text = m_cb_get_text();
    } else
    {
        text = m_name;
    }
    ssd1306_draw_string(p, x, y, 2, text.c_str());
}
