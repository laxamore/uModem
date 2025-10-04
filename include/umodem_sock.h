#ifndef uMODEM_SOCK_H
#define uMODEM_SOCK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "umodem.h"

  typedef enum
  {
    UMODEM_SOCK_TCP,
    UMODEM_SOCK_UDP,
  } umodem_sock_type_t;

  umodem_result_t umodem_sock_init(void);
  umodem_result_t umodem_sock_deinit(void);
  int umodem_sock_create(umodem_sock_type_t type);
  umodem_result_t umodem_sock_connect(int sockfd, const char *host, size_t host_len, uint16_t port, uint32_t timeout_ms);
  umodem_result_t umodem_sock_close(int sockfd);
  int umodem_sock_send(int sockfd, const void *data, size_t len);
  int umodem_sock_recv(int sockfd, void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif