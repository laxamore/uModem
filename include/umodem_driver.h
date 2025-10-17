#ifndef uMODEM_DRIVER_H_
#define uMODEM_DRIVER_H_

#include "umodem.h"
#include "umodem_mqtt.h"
#include "umodem_sock.h"
#include "umodem_event.h"

#ifdef __cplusplus
extern "C" {
#endif

struct umodem_event {
  umodem_event_flag_t event_flag;
  void (*dtor)(umodem_event_t* self);
  void* data;
};

typedef struct {
  umodem_result_t (*sock_init)(void);
  umodem_result_t (*sock_deinit)(void);

  int (*sock_create)(umodem_sock_type_t type);
  umodem_result_t (*sock_connect)(int sockfd, const char* host, size_t host_len,
      uint16_t port, uint32_t timeout_ms);
  umodem_result_t (*sock_close)(int sockfd);
  int (*sock_send)(int sockfd, const uint8_t* data, size_t len);
  int (*sock_recv)(int sockfd, uint8_t* buf, size_t len);
} umodem_sock_driver_t;

typedef struct {
} umodem_http_driver_t;

typedef struct {
  umodem_result_t (*mqtt_init)(void);
  umodem_result_t (*mqtt_deinit)(void);

  int (*mqtt_connect)(
      const char* host, uint16_t port, const umodem_mqtt_connect_opts_t* opts);
  umodem_result_t (*mqtt_disconnect)(int sockfd);
  umodem_result_t (*mqtt_subscribe)(
      int sockfd, const char* topic, size_t topic_len, umodem_mqtt_qos_t qos);
  umodem_result_t (*mqtt_unsubscribe)(
      int sockfd, const char* topic, size_t topic_len);
  umodem_result_t (*mqtt_publish)(int sockfd, const char* topic, size_t topic_len,
      const void* payload, size_t len, umodem_mqtt_qos_t qos, int retain);
} umodem_mqtt_driver_t;

typedef struct {
} umodem_ppp_driver_t;

typedef struct {
  umodem_apn_t* apn;

  umodem_result_t (*init)(void);
  umodem_result_t (*deinit)(void);

  umodem_result_t (*get_imei)(char* buf, size_t buf_size);
  umodem_result_t (*get_iccid)(char* buf, size_t buf_size);
  umodem_result_t (*get_signal)(int* rssi, int* ber);

  umodem_event_t (*handle_urc)(const char* buf, size_t len);

  const umodem_sock_driver_t* sock_driver;
  const umodem_http_driver_t* http_driver;
  const umodem_mqtt_driver_t* mqtt_driver;
  const umodem_ppp_driver_t* ppp_driver;

  uint8_t umodem_initialized;
} umodem_driver_t;

extern umodem_driver_t* g_umodem_driver;

#ifdef __cplusplus
}
#endif

#endif