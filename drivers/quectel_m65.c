#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "umodem_config.h"
#include "umodem_driver.h"
#include "umodem_sock.h"
#include "umodem_hal.h"
#include "umodem_at.h"

#if defined(UMODEM_MODEM_QUECTEL_M65)

static int g_modem_functional = 0;
static int g_sim_inserted = 0;
static int g_data_connected = 0;
static int g_network_attached = 0;

static umodem_result_t check_sim_status(void)
{
  char response[64];
  if (umodem_at_send("AT+CPIN?\r", response, sizeof(response), 2000) != UMODEM_OK)
    return UMODEM_ERR;

  if (strstr(response, "READY"))
  {
    g_sim_inserted = 1;
    return UMODEM_OK;
  }
  else
  {
    g_sim_inserted = 0;
    return UMODEM_SIM_NOT_INSERTED;
  }
}

static umodem_result_t quectel_m65_get_imei(char *buf, size_t buf_size)
{
  if (!g_sim_inserted)
    return UMODEM_SIM_NOT_INSERTED;
  if (!buf || buf_size == 0)
    return UMODEM_PARAM;

  return umodem_at_send("AT+CGSN\r", buf, buf_size, 5000);
}

static int quectel_m65_handle_urc(char *buf, size_t len)
{
  if (strstr(buf, "+PDP DEACT"))
  {
    g_data_connected = 0;
    return UMODEM_NO_EVENT;
  }
  else if (strstr(buf, "+CREG:"))
  {
    char *token = strtok(buf, ",");
    if (token)
      strtok(NULL, ",");
    if (token && (*token == '1' || *token == '5'))
    {
      g_network_attached = 1;
    }
    else
    {
      g_network_attached = 0;
    }
    return UMODEM_NO_EVENT;
  }
  else if (strstr(buf, "CLOSED"))
  {
    return UMODEM_NO_EVENT;
  }
  else if (strstr(buf, "+CMTI:"))
  {
    /* SMS received */
    return UMODEM_NO_EVENT;
  }

  return UMODEM_URC_IGNORE;
}

static umodem_result_t quectel_m65_init()
{
  g_modem_functional = 0;
  g_sim_inserted = 0;
  g_data_connected = 0;
  g_network_attached = 0;

  // Software Restart Modem
  if (umodem_at_send("AT+CFUN=1,1\r", NULL, 0, 1000) != UMODEM_OK)
    return UMODEM_ERR;

  // AT
  uint32_t init_start = umodem_hal_millis();
  while (umodem_hal_millis() - init_start < 10000)
  {
    if (umodem_at_send("AT\r", NULL, 0, 1000) == UMODEM_OK)
    {
      g_modem_functional = 1;
      break;
    }
  }
  if (!g_modem_functional)
    return UMODEM_TIMEOUT;

  // Disable command echo
  if (umodem_at_send("ATE0\r", NULL, 0, 1000) != UMODEM_OK)
    return UMODEM_ERR;

  // Set modem to verbose error mode
  if (umodem_at_send("AT+CMEE=2\r", NULL, 0, 1000) != UMODEM_OK)
    return UMODEM_ERR;

  // Check SIM status
  uint32_t start = umodem_hal_millis();
  while (umodem_hal_millis() - start < 10000)
  {
    if (check_sim_status() == UMODEM_OK)
      break;
    umodem_hal_delay_ms(500);
  }

  if (g_sim_inserted == 0)
    return UMODEM_SIM_NOT_INSERTED;

  // Enable network registration URCs
  if (umodem_at_send("AT+CREG=1\r", NULL, 0, 1000) != UMODEM_OK)
    return UMODEM_ERR;

  char buf[24];
  if (umodem_at_send("AT+CREG?\r", buf, sizeof(buf), 1000) == UMODEM_OK)
  {
    char *token = strtok(buf, ",");
    if (token != NULL)
      strtok(NULL, ",");
    if (token != NULL && *token == '1')
      g_network_attached = 1;
    else
      g_network_attached = 0;
  }

  return UMODEM_OK;
}

static umodem_result_t quectel_m65_deinit()
{
  return UMODEM_OK;
}

static umodem_result_t quectel_m65_get_iccid(char *buf, size_t buf_size)
{
  if (!g_sim_inserted)
    return UMODEM_SIM_NOT_INSERTED;

  if (!buf || buf_size == 0)
    return UMODEM_PARAM;

  return umodem_at_send("AT+QCCID\r", buf, buf_size, 5000);
}

static umodem_result_t quectel_m65_get_signal(int *rssi, int *ber)
{
  if (!g_sim_inserted)
    return UMODEM_SIM_NOT_INSERTED;

  if (!rssi || !ber)
    return UMODEM_PARAM;
  char response[32];
  if (umodem_at_send("AT+CSQ\r", response, sizeof(response),
                     2000) != UMODEM_OK)
    return UMODEM_ERR;
  // +CSQ: <rssi>,<ber>
  char *token = strtok(response, ":");
  if (token)
    token = strtok(NULL, ",");
  if (token)
    *rssi = atoi(token);
  if (token)
    token = strtok(NULL, "\r");
  if (token)
    *ber = atoi(token);
  return UMODEM_OK;
}

static umodem_driver_t s_quectel_m65_driver = {
    .init = quectel_m65_init,
    .deinit = quectel_m65_deinit,
    .get_imei = quectel_m65_get_imei,
    .get_signal = quectel_m65_get_signal,
    .get_iccid = quectel_m65_get_iccid,
    .handle_urc = quectel_m65_handle_urc,
    .sock_driver = NULL,
    .http_driver = NULL,
    .mqtt_driver = NULL,
    .ppp_driver = NULL,
};

umodem_driver_t *g_umodem_driver = &s_quectel_m65_driver;

#endif