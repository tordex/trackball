#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include <functional>

enum class button_state_t
{
    pressed,
    released,
};

class button
{
  private:
    // Button PIN.
    gpio_num_t      m_pin;
    // Button ID.
    uint32_t        m_id;
    // Button's Task ID.
    uint32_t        m_task_id;
    // Current GPIO level.
    int             m_level = 1;
    // Ticks when level was changed. 0 when debounced.
    TickType_t      m_last_changed = 0;
    // Current button state
    button_state_t  m_state = button_state_t::released;
    // Ticks when level was changed. 0 when not pressed. Used to handle click
    TickType_t      m_last_pressed = 0;
    int             m_debounce_ms;
    int             m_click_ms;

  public:
    button(uint32_t id, gpio_num_t pin, uint32_t task_id, int debounce_ms = 10, int click_ms = 200);

    void loop(const std::function<void (button_state_t)>& cb_state_changed, const std::function<void ()>& cb_click = nullptr);

    uint32_t get_task_id() const { return m_task_id; }
    uint32_t get_id() const { return m_id; }
};

#endif // __BUTTON_H__
