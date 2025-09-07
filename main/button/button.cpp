#include "button.h"
#include "freertos/task.h"
#include <thread>
#include <mutex>
#include "esp_log.h"

QueueHandle_t button::m_buttons_queue = nullptr;

static inline button_state_t level_to_state(int level)
{
    return level == 0 ? button_state_t::pressed : button_state_t::released;
}

static void IRAM_ATTR btn_isr_handler(void *arg)
{
    auto btn = static_cast<button*>(arg);
    btn->send_event_from_isr(btn_event_t::isr);
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

    static std::once_flag button_task_flag;
    std::call_once(button_task_flag, [this]() {
        m_buttons_queue = xQueueCreate(10, sizeof(button_event_t));
        xTaskCreate(button::buttons_task, "buttons_task", 4096, nullptr, 5, nullptr);
    });

    gpio_isr_handler_add(m_pin, btn_isr_handler, (void *)this);

    m_debounce_timer = xTimerCreate("debounce_timer", pdMS_TO_TICKS(m_debounce_ms), pdFALSE, this, [](TimerHandle_t xTimer)
    {
        auto btn = static_cast<button*>(pvTimerGetTimerID(xTimer));
        btn->send_event(btn_event_t::debounce_timer);
    });
}

void button::buttons_task(void*)
{
    button_event_t btn_event;
    while (true)
    {
        if (xQueueReceive(m_buttons_queue, &btn_event, portMAX_DELAY))
        {
            switch (btn_event.event)
            {
            case btn_event_t::isr:
                {
                    int level = gpio_get_level(btn_event.btn->m_pin);
                    if (level != btn_event.btn->m_level)
                    {
                        btn_event.btn->m_level = level;
                        // Level is changed. Starting debounce.
                        xTimerReset(btn_event.btn->m_debounce_timer, 0);
                    }
                }
                break;

            case btn_event_t::debounce_timer:
                {
                    int level = gpio_get_level(btn_event.btn->m_pin);

                    //Stop debounce.
                    xTimerStop(btn_event.btn->m_debounce_timer, 0);
                    btn_event.btn->m_last_changed = 0;
                    auto new_state = level_to_state(btn_event.btn->m_level);
                    if(new_state != btn_event.btn->m_state)
                    {
                        btn_event.btn->m_state = new_state;
                        if(btn_event.btn->m_cb_state_changed)
                        {
                            btn_event.btn->m_cb_state_changed(btn_event.btn->m_state);
                        }
                        if(btn_event.btn->m_state == button_state_t::pressed)
                        {
                            btn_event.btn->m_last_pressed = xTaskGetTickCount();
                        } else if(btn_event.btn->m_last_pressed != 0 && (xTaskGetTickCount() - btn_event.btn->m_last_pressed) <= pdMS_TO_TICKS(btn_event.btn->m_click_ms))
                        {
                            btn_event.btn->m_last_pressed = 0;
                            if(btn_event.btn->m_cb_click)
                            {
                                btn_event.btn->m_cb_click();
                            }
                        }
                    }
                }
                break;

            default:
                break;
            }
        }
    }
}
