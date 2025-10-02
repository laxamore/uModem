#ifndef uMODEM_DRIVER_H_
#define uMODEM_DRIVER_H_

#include "umodem.h"
#include "umodem_sock.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
  } umodem_sock_driver_t;

  typedef struct
  {
  } umodem_http_driver_t;

  typedef struct
  {
  } umodem_mqtt_driver_t;

  typedef struct
  {
  } umodem_ppp_driver_t;

  typedef struct
  {
    umodem_apn_t *apn;

    umodem_result_t (*init)(void);
    umodem_result_t (*deinit)(void);

    umodem_result_t (*get_imei)(char *buf, size_t buf_size);
    umodem_result_t (*get_iccid)(char *buf, size_t buf_size);
    umodem_result_t (*get_signal)(int *rssi, int *ber);

    int (*handle_urc)(char *buf, size_t len);

    const umodem_sock_driver_t *sock_driver;
    const umodem_http_driver_t *http_driver;
    const umodem_mqtt_driver_t *mqtt_driver;
    const umodem_ppp_driver_t *ppp_driver;

    uint8_t umodem_initialized;
  } umodem_driver_t;

  extern umodem_driver_t *g_umodem_driver;

#ifdef __cplusplus
}
#endif

#endif