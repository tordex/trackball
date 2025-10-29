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

enum class btn_event_t
{
	isr,
	debounce_timer,
	hold_down_timer
};

class button;

struct button_event_t
{
	button*		btn;
	btn_event_t event;
};

class button
{
private:
	// Button PIN.
	gpio_num_t m_pin			 = GPIO_NUM_NC;
	// Button ID.
	uint32_t m_id				 = 0;
	// Button's Task ID.
	uint32_t m_task_id			 = 0;
	// Current GPIO level.
	int m_level					 = 1;
	// Ticks when level was changed. 0 when debounced.
	TickType_t m_last_changed	 = 0;
	// Current button state
	button_state_t m_state		 = button_state_t::released;
	// Ticks when level was changed. 0 when not pressed. Used to handle click
	TickType_t	  m_last_pressed = 0;
	int			  m_debounce_ms;
	int			  m_click_ms;
	TimerHandle_t m_debounce_timer = nullptr;

	std::function<void(button_state_t)> m_cb_state_changed;
	std::function<void()>				m_cb_click;

	static QueueHandle_t m_buttons_queue;
public:
	button(gpio_num_t pin, int debounce_ms = 10, int click_ms = 200);

	void set_cb_on_state_changed(const std::function<void(button_state_t)>& cb_state_changed)
	{
		m_cb_state_changed = cb_state_changed;
	}

	void set_cb_on_click(const std::function<void()>& cb_click)
	{
		m_cb_click = cb_click;
	}

	uint32_t get_task_id() const
	{
		return m_task_id;
	}
	uint32_t get_id() const
	{
		return m_id;
	}

	void send_event_from_isr(btn_event_t event)
	{
		BaseType_t	   xHigherPriorityTaskWoken = pdFALSE;
		button_event_t btn_event				= {this, event};
		xQueueSendFromISR(m_buttons_queue, &btn_event, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}

	void send_event(btn_event_t event)
	{
		button_event_t btn_event = {this, event};
		xQueueSend(m_buttons_queue, &btn_event, portMAX_DELAY);
	}

private:
	static void buttons_task(void* pvParameters);
};

#endif // __BUTTON_H__
