#include "umodem_sock.h"
#include "umodem_driver.h"

umodem_result_t umodem_sock_init(void)
{
  if (g_umodem_driver == NULL || g_umodem_driver->sock_driver == NULL || g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->sock_driver->sock_init();
}

umodem_result_t umodem_sock_deinit(void)
{
  if (g_umodem_driver == NULL || g_umodem_driver->sock_driver == NULL || g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->sock_driver->sock_deinit();
}

int umodem_sock_create(umodem_sock_type_t type)
{
  if (g_umodem_driver == NULL || g_umodem_driver->sock_driver == NULL || g_umodem_driver->umodem_initialized == 0 ||
      g_umodem_driver->sock_driver->sock_create == NULL)
    return -1;

  return g_umodem_driver->sock_driver->sock_create(type);
}

umodem_result_t umodem_sock_connect(int sockfd, const char *host, size_t host_len, uint16_t port, uint32_t timeout_ms)
{
  if (g_umodem_driver == NULL || g_umodem_driver->sock_driver == NULL || g_umodem_driver->umodem_initialized == 0 ||
      g_umodem_driver->sock_driver->sock_connect == NULL)
    return UMODEM_ERR;

  return g_umodem_driver->sock_driver->sock_connect(sockfd, host, host_len, port, timeout_ms);
}

umodem_result_t umodem_sock_close(int sockfd)
{
  if (g_umodem_driver == NULL || g_umodem_driver->sock_driver == NULL || g_umodem_driver->umodem_initialized == 0 ||
      g_umodem_driver->sock_driver->sock_close == NULL)
    return UMODEM_ERR;

  return g_umodem_driver->sock_driver->sock_close(sockfd);
}

int umodem_sock_send(int sockfd, const void *data, size_t len)
{
  if (!g_umodem_driver || !g_umodem_driver->sock_driver || !g_umodem_driver->umodem_initialized ||
      !g_umodem_driver->sock_driver->sock_send)
    return -1;
  return g_umodem_driver->sock_driver->sock_send(sockfd, data, len);
}

int umodem_sock_recv(int sockfd, void *buf, size_t len)
{
  if (!g_umodem_driver || !g_umodem_driver->sock_driver || !g_umodem_driver->umodem_initialized ||
      !g_umodem_driver->sock_driver->sock_recv)
    return -1;
  return g_umodem_driver->sock_driver->sock_recv(sockfd, buf, len);
}