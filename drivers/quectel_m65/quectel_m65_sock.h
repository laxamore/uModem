#ifndef uMODEM_DRIVER_QUECTEL_M65_SOCK_H_
#define uMODEM_DRIVER_QUECTEL_M65_SOCK_H_

#include "umodem_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QUECTEL_M65_MAX_SOCKETS 6

#define QIMUX_TIMEOUT_MS (1000 * 2)
#define QINDI_TIMEOUT_MS (1000 * 2)
#define QIREGAPP_TIMEOUT_MS (1000 * 5)
#define QIACT_TIMEOUT_MS (1000 * 150)
#define QIDEACT_TIMEOUT_MS (1000 * 40)
#define NETWORK_ATTACH_TIMEOUT_MS (1000 * 90)

#define QIOPEN_TIMEOUT_MS (1000 * 10)
#define QICLOSE_TIMEOUT_MS (1000 * 5)
#define QISEND_CMD_TIMEOUT_MS (1000 * 5)
#define QISEND_DATA_TIMEOUT_MS (1000 * 10)
#define QIRD_TIMEOUT_MS (1000 * 5)

#define QIRD_MAX_RECV_LEN 1500 // MTU-sized
#define QISEND_MAX_SEND_LEN 1460

// === Socket State ===
typedef struct {
  int sockfd;
  umodem_sock_type_t type;
  int connected;
} quectel_m65_socket_t;

// Socket
extern const umodem_sock_driver_t quectel_m65_sock_driver;
extern quectel_m65_socket_t g_sockets[QUECTEL_M65_MAX_SOCKETS];

#ifdef __cplusplus
}
#endif

#endif
