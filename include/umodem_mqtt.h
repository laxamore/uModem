#ifndef uMODEM_MQTT_H_
#define uMODEM_MQTT_H_

#include "umodem.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum
  {
    UMODEM_MQTT_QOS_0 = 0,
    UMODEM_MQTT_QOS_1 = 1,
    UMODEM_MQTT_QOS_2 = 2,
  } umodem_mqtt_qos_t;

  typedef struct
  {
    const char *topic;     // Topic to publish the Will message
    const char *message;   // Payload of the Will message
    umodem_mqtt_qos_t qos; // QoS level (0, 1, or 2)
    int retain;            // Retain flag
  } umodem_mqtt_will_t;

  typedef struct
  {
    const char *client_id;
    const char *username;
    const char *password;
    uint16_t keepalive; // In seconds, if 0 the client will not be disconnected

    const umodem_mqtt_will_t
        *will; // Pointer to Will configuration (NULL if unused)

    const uint8_t delivery_timeout_in_seconds;

    const uint8_t disable_clean_session;

    const uint8_t ssl_enable;
  } umodem_mqtt_connect_opts_t;

  umodem_result_t umodem_mqtt_init(void);
  umodem_result_t umodem_mqtt_deinit(void);

  int umodem_mqtt_connect(const char *host, uint16_t port,
                          const umodem_mqtt_connect_opts_t *opts);

  umodem_result_t umodem_mqtt_disconnect(int sockfd);

  umodem_result_t umodem_mqtt_subscribe(int sockfd, const char *topic,
                                        umodem_mqtt_qos_t qos);

  umodem_result_t umodem_mqtt_unsubscribe(int sockfd, const char *topic);

  umodem_result_t umodem_mqtt_publish(int sockfd, const char *topic,
                                      const void *payload, size_t len,
                                      umodem_mqtt_qos_t qos, int retain);

#ifdef __cplusplus
}
#endif
#endif
