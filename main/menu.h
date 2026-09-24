#ifndef _MENU_H
#define _MENU_H

#include <string>
#include <vector>
#include <functional>
#include "oled/ssd1306.h"
#include <memory>

namespace ui_menu
{
    const int ITEMS_ON_SCREEN = 4;

    enum confirm_result
    {
        CONFIRM_PROCESSES,
        CONFIRM_GRAB_FOCUS,
        CONFIRM_GO_BACK
    };

    class menu_item
    {
      private:
        std::string_view             m_name;
        std::function<void()>        m_cb_confirm;
        std::function<std::string()> m_cb_get_marker;
        std::function<std::string()> m_cb_get_text;

      public:
        menu_item(std::string_view name, std::function<void()> cb_confirm,
                  std::function<std::string()> cb_get_marker = nullptr) :
            m_name(name),
            m_cb_confirm(cb_confirm),
            m_cb_get_marker(cb_get_marker)
        {
        }
        menu_item(std::function<std::string()> cb_get_text, std::function<void()> cb_confirm,
                  std::function<std::string()> cb_get_marker = nullptr) :
            m_name(""),
            m_cb_confirm(cb_confirm),
            m_cb_get_marker(cb_get_marker),
            m_cb_get_text(cb_get_text)
        {
        }

        ~menu_item() {}

        confirm_result on_confirm(bool is_focused)
        {
            if(m_cb_confirm)
            {
                m_cb_confirm();
            }
            return CONFIRM_GO_BACK;
        }

        bool on_back()
        {
            return false;
        }
        bool on_scroll(int step)
        {
            return false;
        }
        void on_draw_focused(ssd1306_t* p, int x, int y) {}
        void on_draw(ssd1306_t* p, int x, int y);
    };

    class value_editor
    {
        std::string_view m_name;
        std::string_view m_sub_title;

        std::function<void(int)>     m_cb_confirm;
        std::function<std::string()> m_cb_get_text;

        int m_value;
        int m_current_value;
        int m_min;
        int m_max;
        int m_step;

      public:
        value_editor(std::string_view name, std::string_view sub_title, int value, int min, int max, int step,
                     std::function<void(int)> cb_confirm, std::function<std::string()> cb_get_text = nullptr) :
            m_name(name),
            m_sub_title(sub_title),
            m_cb_confirm(cb_confirm),
            m_cb_get_text(cb_get_text),
            m_value(value),
            m_current_value(value),
            m_min(min),
            m_max(max),
            m_step(step)
        {
        }

        ~value_editor() {}

        confirm_result on_confirm(bool is_focused);

        bool on_back()
        {
            return true;
        }

        bool on_scroll(int step);

        void on_draw_focused(ssd1306_t* p, int x, int y);
        void on_draw(ssd1306_t* p, int x, int y);
    };

    class submenu
    {
      public:
        using child_obj = std::variant<menu_item, submenu, value_editor>;

        submenu(std::string_view name, const std::vector<child_obj>& items) :
            m_name(name),
            m_items(items)
        {
        }
        submenu(std::function<std::string()> cb_get_text, const std::vector<child_obj>& items) :
            m_name(""),
            m_items(items),
            m_cb_get_text(cb_get_text)
        {
        }

        ~submenu() {}

        confirm_result on_confirm(bool is_focused);
        bool           on_back();
        bool           on_scroll(int step);
        void           on_draw_focused(ssd1306_t* p, int x, int y);
        void           on_draw(ssd1306_t* p, int x, int y);

      private:
        int  clamp_menu_value(int new_index);
        bool update_menu_top_idx();

        std::string_view             m_name;
        std::vector<child_obj>       m_items;
        std::function<std::string()> m_cb_get_text;

        int m_current_index = 0;
        int m_focused_index = -1;
        int m_top_index     = 0;
    };

} // namespace ui_menu

#endif // _MENU_H
