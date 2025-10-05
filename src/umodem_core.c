#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "umodem.h"
#include "umodem_at.h"
#include "umodem_hal.h"
#include "umodem_buffer.h"
#include "umodem_config.h"
#include "umodem_driver.h"

#define MAX_QUEUED_EVENTS 10

// User event callback
static umodem_event_cb_t g_event_cb = NULL;
static void *g_user_ctx = NULL;
static size_t urc_scan_offset = 0;

// URC handler function
typedef umodem_event_info_t (*umodem_urc_handler_t)(const uint8_t *line, size_t len);
static umodem_event_info_t g_event_queue[MAX_QUEUED_EVENTS];
static size_t g_event_queue_len = 0;

static void queue_event(umodem_event_info_t event)
{
  if (g_event_queue_len < MAX_QUEUED_EVENTS && event.event != UMODEM_NO_EVENT && event.event != UMODEM_URC_IGNORE)
    g_event_queue[g_event_queue_len++] = event;
}

static void dispatch_queued_events(void)
{
  while (g_event_queue_len > 0)
  {
    umodem_event_info_t event = g_event_queue[g_event_queue_len - 1];
    g_event_queue_len--;
    if (g_event_cb)
      g_event_cb(&event, g_user_ctx);
  }
}

umodem_result_t umodem_init(umodem_apn_t *apn)
{
  umodem_result_t result = UMODEM_OK;

  if (g_umodem_driver->init == NULL || g_umodem_driver->deinit == NULL || g_umodem_driver->get_imei == NULL ||
      g_umodem_driver->get_iccid == NULL || g_umodem_driver->get_signal == NULL)
    return UMODEM_ERR;

  if (g_umodem_driver->umodem_initialized == 1)
    return result;

  umodem_buffer_init(&urc_scan_offset);
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

static int umodem_buffer_process_urcs(umodem_urc_handler_t handler)
{
  if (!handler)
    return -1;

  int lines_processed = 0;
  size_t offset = urc_scan_offset;

  while (offset < umodem_buffer_get_count())
  {
    int pos = umodem_buffer_find_from((uint8_t *)"\r\n", 2, offset);
    if (pos < 0)
      break;

    size_t line_len = (size_t)(pos - offset) + 2;
    char buf[line_len + 1];

    if (umodem_buffer_peek_from((uint8_t *)buf, offset, line_len) != (int)line_len)
      break;

    buf[line_len] = '\0';

    // Call handler
    umodem_event_info_t event_info = handler((uint8_t *)buf, line_len);
    queue_event(event_info);

    lines_processed++;
    offset = pos + 2;
  }

  urc_scan_offset = offset;
  return lines_processed;
}

void umodem_poll(void)
{
  if (g_umodem_driver->umodem_initialized == 0)
    return;

  umodem_hal_lock();

  // Read new data
  uint8_t read_buf[UMODEM_RX_BUF_SIZE / 2];
  size_t len = umodem_hal_read(read_buf, sizeof(read_buf));
  if (len > 0)
    umodem_buffer_push(read_buf, len);

  // Process all new URC lines
  umodem_buffer_process_urcs(g_umodem_driver->handle_urc);

  umodem_hal_unlock();

  dispatch_queued_events();
}