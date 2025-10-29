#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

class timer
{
public:
	using callback_t = std::function<void()>;
private:
	TimerHandle_t m_timer = nullptr;
	callback_t	  m_callback;
	std::string	  m_name;
public:

	timer(const char* name) :
		m_name(name)
	{
	}

	~timer()
	{
		if(m_timer)
		{
			xTimerDelete(m_timer, 0);
		}
	}

	void set_callback(callback_t cb)
	{
		m_callback = cb;
	}

	void start(uint32_t duration_ms, bool auto_reload, callback_t cb = nullptr)
	{
		if(!m_timer)
		{
			if(cb)
			{
				m_callback = cb;
			}
			m_timer = xTimerCreate(m_name.c_str(), pdMS_TO_TICKS(duration_ms), auto_reload ? pdTRUE : pdFALSE, this,
								   [](TimerHandle_t xTimer) {
									   timer* t = static_cast<timer*>(pvTimerGetTimerID(xTimer));
									   if(t && t->m_timer && t->m_callback)
									   {
										   t->m_callback();
									   }
								   });

			xTimerStart(m_timer, 0);
		} else
		{
			xTimerChangePeriod(m_timer, pdMS_TO_TICKS(duration_ms), 0);
		}
	}

	void reset()
	{
		if(m_timer)
		{
			xTimerReset(m_timer, 0);
		}
	}

	void stop()
	{
		if(m_timer)
		{
			xTimerStop(m_timer, 0);
			xTimerDelete(m_timer, 0);
			m_timer = nullptr;
		}
	}
};
