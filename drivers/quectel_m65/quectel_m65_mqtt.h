#ifndef uMODEM_DRIVER_QUECTEL_M65_MQTT_H_
#define uMODEM_DRIVER_QUECTEL_M65_MQTT_H_

#include "quectel_m65_sock.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QMTCFG_TIMEOUT_MS (1000 * 5)
#define QMTOPEN_TIMEOUT_MS (1000 * 75)
#define QMTCONN_TIMEOUT_MS (1000 * 60)

#define QUECTEL_M65_MAX_MQTT_CONNS QUECTEL_M65_MAX_SOCKETS

typedef struct {
  quectel_m65_socket_t sock;
  int context_open;
} quectel_m65_mqtt_conn_t;

uint16_t mqtt_add_message(uint8_t sockfd, const char* topic, size_t topic_len,
    const uint8_t* payload, size_t len, uint8_t type);
umodem_event_mqtt_data_t* mqtt_pop_message(uint16_t msg_id);
umodem_event_mqtt_data_t* mqtt_get_message(uint16_t msg_id);
umodem_event_mqtt_data_t* mqtt_get_message_by_topic(const char* topic, size_t topic_len);

void umodem_event_mqtt_pub_dtor(umodem_event_t* self);
void umodem_event_mqtt_sub_dtor(umodem_event_t* self);

extern const umodem_mqtt_driver_t quectel_m65_mqtt_driver;
extern quectel_m65_mqtt_conn_t g_mqtt_conns[QUECTEL_M65_MAX_MQTT_CONNS];

#ifdef __cplusplus
}
#endif

#endif
