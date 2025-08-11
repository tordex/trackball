#include "button.h"
#include "freertos/task.h"

static inline button_state_t level_to_state(int level)
{
    return level == 0 ? button_state_t::pressed : button_state_t::released;
}

button::button(uint32_t id, gpio_num_t pin, uint32_t task_id, int debounce_ms /* = 10 */, int click_ms /* = 100 */) :
        m_pin(pin),
        m_id(id),
        m_task_id(task_id),
        m_debounce_ms(debounce_ms),
        m_click_ms(click_ms)
{
    gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << m_pin);
        io_conf.mode         = GPIO_MODE_INPUT;
        io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type    = GPIO_INTR_ANYEDGE;
    gpio_config(&io_conf);
}

void button::loop(const std::function<void (button_state_t)>& cb_state_changed, const std::function<void ()>& cb_click /* = nullptr */)
{
    int level = gpio_get_level((gpio_num_t) m_pin);
    TickType_t now = xTaskGetTickCount();
    if (level != m_level)
    {
        // Level is changed. Starting debounce.
        m_level = level;
        m_last_changed = now;
    } else if (m_last_changed != 0 && (now - m_last_changed) >= pdMS_TO_TICKS(m_debounce_ms))
    {
        //Stop debounce.
        m_last_changed = 0;
        auto new_state = level_to_state(m_level);
        if(new_state != m_state)
        {
            m_state = new_state;
            if(cb_state_changed)
            {
                cb_state_changed(m_state);
            }
            if(m_state == button_state_t::pressed)
            {
                m_last_pressed = now;
            } else if(m_last_pressed != 0 && (now - m_last_pressed) <= pdMS_TO_TICKS(m_click_ms))
            {
                m_last_pressed = 0;
                if(cb_click)
                {
                    cb_click();
                }
            }
        }
    }
}
