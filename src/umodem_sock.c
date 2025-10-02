#include "umodem_sock.h"
#include "umodem_driver.h"

umodem_result_t umodem_sock_init(void)
{
  if (g_umodem_driver == NULL || g_umodem_driver->sock_driver == NULL || g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  // TODO: Initialize socket driver
  return UMODEM_ERR; // Not implemented yet
}