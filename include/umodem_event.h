#ifndef uMODEM_EVENT_H_
#define uMODEM_EVENT_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  UMODEM_NO_EVENT = 0,
  UMODEM_EVENT_DATA_DOWN = 1,    // Data connection deactivated
  UMODEM_EVENT_SMS_RECEIVED = 2, // New SMS received (if SMS monitoring enabled)
  UMODEM_EVENT_SOCK_CONNECTED = 3,      // Socket successfully connected
  UMODEM_EVENT_SOCK_CLOSED = 4,         // Socket closed
  UMODEM_EVENT_SOCK_DATA_RECEIVED = 5,  // Data available to read on socket
  UMODEM_EVENT_MQTT_DATA_PUBLISHED = 6, // Data available to read on socket
  UMODEM_EVENT_MQTT_DATA_RECEIVED = 7,  // Data available to read on socket
} umodem_event_flag_t;

typedef struct umodem_event umodem_event_t;

typedef struct {
  uint8_t sockfd;
  uint16_t id;
  const char* topic;
  size_t topic_len;
  uint8_t* data;
  size_t data_len;
} umodem_event_mqtt_data_t;

umodem_event_flag_t umodem_event_get_flag(umodem_event_t* event);
void* umodem_get_event_data(umodem_event_t* event);

#ifdef __cplusplus
}
#endif

#endif
