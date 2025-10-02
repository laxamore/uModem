#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "umodem.h"
#include "umodem_at.h"
#include "umodem_hal.h"
#include "umodem_buffer.h"
#include "umodem_config.h"
#include "umodem_driver.h"

// User event callback
static umodem_event_cb_t g_event_cb = NULL;
static void *g_user_ctx = NULL;

static void handle_event(int event)
{
  switch (event)
  {
  case UMODEM_EVENT_URC:
    break;
  case UMODEM_EVENT_NET_UP:
    break;
  case UMODEM_EVENT_NET_DOWN:
    break;
  case UMODEM_EVENT_DATA_UP:
    break;
  case UMODEM_EVENT_DATA_DOWN:
    break;
  case UMODEM_EVENT_SMS_RECEIVED:
    break;
  default:
    break;
  }
}

umodem_result_t umodem_init(umodem_mode_t mode, umodem_apn_t *apn)
{
  umodem_result_t result = UMODEM_OK;

  if (g_umodem_driver->init == NULL || g_umodem_driver->deinit == NULL || g_umodem_driver->get_imei == NULL ||
      g_umodem_driver->get_iccid == NULL || g_umodem_driver->get_signal == NULL)
    return UMODEM_ERR;

  if (g_umodem_driver->umodem_initialized == 1)
    return result;

  umodem_at_init();

  result = g_umodem_driver->init();
  if (result == UMODEM_OK)
    g_umodem_driver->umodem_initialized = 1;
  else
    umodem_at_deinit();

  g_umodem_driver->apn = apn;
  return result;
}

umodem_result_t umodem_deinit(void)
{
  umodem_result_t result = UMODEM_OK;
  if (!g_umodem_driver->umodem_initialized)
    return result;

  result = g_umodem_driver->deinit();
  if (result != UMODEM_OK)
    return result;

  umodem_at_deinit();
  g_umodem_driver->umodem_initialized = 0;
  return result;
}

umodem_result_t umodem_power_on(void)
{
  return UMODEM_OK;
}

umodem_result_t umodem_power_off(void)
{
  return UMODEM_OK;
}

umodem_result_t umodem_get_imei(char *buf, size_t buf_size)
{
  if (!g_umodem_driver->umodem_initialized)
    return UMODEM_ERR;

  return g_umodem_driver->get_imei(buf, buf_size);
}

umodem_result_t umodem_get_iccid(char *buf, size_t buf_size)
{
  if (!g_umodem_driver->umodem_initialized)
    return UMODEM_ERR;

  return g_umodem_driver->get_iccid(buf, buf_size);
}

umodem_result_t umodem_get_signal_quality(int *rssi, int *ber)
{
  if (!g_umodem_driver->umodem_initialized)
    return UMODEM_ERR;

  return g_umodem_driver->get_signal(rssi, ber);
}

void umodem_register_event_callback(umodem_event_cb_t cb, void *user_ctx)
{
  g_event_cb = cb;
  g_user_ctx = user_ctx;
}

void umodem_poll(void)
{
  if (g_umodem_driver->umodem_initialized == 0)
    return;

  umodem_hal_lock();

  // Support synchronous HAL implementations
  uint8_t read_buf[UMODEM_RX_BUF_SIZE / 2];
  size_t len = 0;
  if ((len = umodem_hal_read(read_buf, sizeof(read_buf))) > 0)
    umodem_buffer_push(read_buf, len);

  // Process first complete line (if any)
  int pos = umodem_buffer_find((uint8_t *)"\r\n", 2);
  if (pos >= 0)
  {
    size_t line_len = (size_t)pos + 2;
    char buf[line_len + 1];
    umodem_buffer_peek((uint8_t *)buf, line_len);
    buf[line_len] = '\0';

    int event = g_umodem_driver->handle_urc((uint8_t *)buf, line_len);
    if (event != UMODEM_URC_IGNORE)
    {
      umodem_buffer_pop(NULL, line_len);
      handle_event((umodem_event_t)event);
    }
  }

  umodem_hal_unlock();
}