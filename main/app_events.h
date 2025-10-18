#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define APP_EVENT_NONE		   0
#define APP_EVENT_SEND_BUTTONS 1
#define APP_EVENT_CONNECTION   2
#define APP_EVENT_BATTERY	   3
#define APP_EVENT_LOCK_STATE   4

	void send_app_event(uint32_t event_id);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // APP_EVENTS_H
