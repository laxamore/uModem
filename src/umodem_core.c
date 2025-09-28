#include <string.h>
#include <stdint.h>

#include "umodem.h"
#include "umodem_hal.h"
#include "umodem_buffer.h"

static int g_modem_ready = 0;
static int g_data_connected = 0;

// User event callback
static umodem_event_cb_t g_event_cb = NULL;
static void *g_user_ctx = NULL;

static void handle_urc(uint8_t* buff, size_t len)
{

}

umodem_result_t umodem_init(uint32_t timeout_ms)
{
  uint32_t init_start = umodem_hal_millis();

  g_modem_ready = 0;

  umodem_buffer_init();
  umodem_hal_init();

  char cmd[8] = "ATE0\r";
  if (umodem_hal_send((const uint8_t *)cmd, strlen(cmd)) < 0)
    return UMODEM_ERR;

  char match[] = "\nOK\n";
  int pos = -1;
  while (umodem_hal_millis() - init_start <= timeout_ms)
  {
    umodem_hal_lock();
    if ((pos = umodem_buffer_find("\nOK\n")) >= 0)
    {
      size_t len = pos + (sizeof(match)-1);
      char buf[len];
      umodem_buffer_pop(buf, len);

      if (len > sizeof(match)-1)
        handle_urc(buf, len - (sizeof(match)-1));

      umodem_hal_unlock();
      return UMODEM_OK;
    }

    umodem_hal_unlock();
    umodem_hal_delay_ms(10);
  }

  return UMODEM_TIMEOUT;
}

umodem_result_t umodem_deinit(void)
{
  return umodem_power_off();
}

void umodem_poll(void)
{
  if (umodem_hal_read() < 0) return;
}

umodem_result_t umodem_power_on(void)
{
  umodem_hal_set_power_on();
  g_modem_ready = 0;
  return UMODEM_OK;
}

umodem_result_t umodem_power_off(void)
{
  umodem_hal_set_power_off();
  g_modem_ready = 0;
  g_data_connected = 0;
  return 0;
}

int umodem_is_ready(void)
{
  // const char cmd[] = "AT+CPIN?\r";
  // if (umodem_hal_send((const uint8_t *)cmd, strlen(cmd)) < 0)
  //   return UMODEM_ERR;

  return g_modem_ready;
}

umodem_result_t umodem_get_imei(char *buf, size_t buf_size)
{
  if (!g_modem_ready)
    return UMODEM_ERR;
  if (!buf || buf_size == 0)
    return UMODEM_PARAM;

  // // Send command
  // const char cmd[] = "AT+CGSN\r";
  // if (umodem_hal_send((const uint8_t *)cmd, strlen(cmd)) < 0)
  //   return UMODEM_ERR;

  return UMODEM_ERR;
}

umodem_result_t umodem_attach_network(uint32_t timeout_ms)
{
  return UMODEM_ERR;
}

umodem_result_t umodem_data_connect(const char *apn)
{
  return UMODEM_ERR;
}

umodem_result_t umodem_data_disconnect(void)
{
  return UMODEM_ERR;
}

int umodem_is_data_connected(void)
{
  return g_data_connected;
}

void umodem_register_event_callback(umodem_event_cb_t cb, void *user_ctx)
{
}