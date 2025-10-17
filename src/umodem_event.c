#include "umodem_event.h"
#include "umodem_driver.h"

umodem_event_flag_t umodem_event_get_flag(umodem_event_t *event)
{
  return event->event_flag;
}

void* umodem_get_event_data(umodem_event_t *event)
{
  return event->data;
}