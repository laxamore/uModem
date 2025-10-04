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

#define QIREGAPP_TIMEOUT_MS (1000 * 5)        // 5 seconds
#define QIACT_TIMEOUT_MS (1000 * 150)         // 150 seconds
#define QIDEACT_TIMEOUT_MS (1000 * 40)        // 40 seconds
#define NETWORK_ATTACH_TIMEOUT_MS (1000 * 90) // 90 seconds

#define QUECTEL_M65_MAX_SOCKETS 6

typedef struct
{
  int sockfd;
  umodem_sock_type_t type;
  int connected;
} quectel_m65_sockfd;

static quectel_m65_sockfd g_sockets[QUECTEL_M65_MAX_SOCKETS] = {0};

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

static int quectel_m65_handle_urc(const uint8_t *buf, size_t len)
{
  char dup_buf[len + 1];
  strncpy(dup_buf, buf, len);
  dup_buf[len] = '\0';

  if (strstr(buf, "+PDP DEACT"))
  {
    g_data_connected = 0;
    return UMODEM_EVENT_DATA_DOWN;
  }
  else if (strstr(buf, "+CREG:"))
  {
    // If buf containing ',' then it's not URC +CREG: <stat>
    if (strchr(dup_buf, ','))
      return UMODEM_URC_IGNORE;

    // +CREG: <stat>
    char *token = strtok(dup_buf, ":");
    if (token)
      token = strtok(NULL, "\r");
    if (token)
    {
      int stat = atoi(token);
      if (stat == 1 || stat == 5)
        g_network_attached = 1;
      else
        g_network_attached = 0;
    }
    return UMODEM_NO_EVENT;
  }
  else if (strstr(buf, "+CMTI:"))
    return UMODEM_EVENT_SMS_RECEIVED; /* SMS received */
  else if (strstr(buf, "CONNECT OK"))
  {
    // <index>, CONNECT OK
    char *token = strtok(dup_buf, ",");
    if (token)
    {
      int index = atoi(token);
      if (index >= 0 && index < QUECTEL_M65_MAX_SOCKETS)
      {
        g_sockets[index].connected = 1; // Mark socket as connected
        return UMODEM_EVENT_SOCK_CONNECTED;
      }
    }
    return UMODEM_NO_EVENT;
  }
  else if (strstr(buf, "CONNECT FAIL"))
  {
    // <index>, CONNECT FAIL
    char *token = strtok(dup_buf, ",");
    if (token)
    {
      int index = atoi(token);
      if (index >= 0 && index < QUECTEL_M65_MAX_SOCKETS)
        g_sockets[index].connected = -1; // Mark socket as failed
    }
    return UMODEM_NO_EVENT;
  }
  else if (strstr(buf, "CLOSED"))
  {
    // <index>, CLOSED
    char *token = strtok(dup_buf, ",");
    if (token)
    {
      int index = atoi(token);
      if (index >= 0 && index < QUECTEL_M65_MAX_SOCKETS)
      {
        g_sockets[index].connected = 0;
        g_sockets[index].sockfd = 0;
        return UMODEM_EVENT_SOCK_CLOSED;
      }
    }
    return UMODEM_NO_EVENT;
  }
  else if (strstr(buf, "+QIRDI:"))
    return UMODEM_EVENT_SOCK_DATA_RECEIVED;

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

  // Wait for modem to restart
  umodem_hal_delay_ms(4000);

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

static umodem_result_t quectel_m65_sock_init()
{
  if (!g_sim_inserted)
    return UMODEM_SIM_NOT_INSERTED;

  uint32_t start = umodem_hal_millis();
  while (umodem_hal_millis() - start < NETWORK_ATTACH_TIMEOUT_MS && !g_network_attached)
  {
    umodem_poll();
    umodem_hal_delay_ms(1000);
  }

  if (!g_network_attached)
    return UMODEM_ERR;

  // Enable QIMUX
  if (umodem_at_send("AT+QIMUX=1\r", NULL, 0, 1000) != UMODEM_OK)
    return UMODEM_ERR;

  // Enable QINDI
  if (umodem_at_send("AT+QINDI=1\r", NULL, 0, 1000) != UMODEM_OK)
    return UMODEM_ERR;

  // AT+QIREGAPP=<APN>,<USER>,<PWD>[,<rate>]
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "AT+QIREGAPP=\"%s\",\"%s\",\"%s\"\r",
           g_umodem_driver->apn->apn,
           g_umodem_driver->apn->user ? g_umodem_driver->apn->user : "",
           g_umodem_driver->apn->pass ? g_umodem_driver->apn->pass : "");

  start = umodem_hal_millis();
  while (umodem_hal_millis() - start < QIREGAPP_TIMEOUT_MS)
  {
    if (umodem_at_send(cmd, NULL, 0, QIREGAPP_TIMEOUT_MS) == UMODEM_OK)
      break;
    umodem_hal_delay_ms(1000);
  }

  // AT+QIACT
  start = umodem_hal_millis();
  while (umodem_hal_millis() - start < QIACT_TIMEOUT_MS)
  {
    if (umodem_at_send("AT+QIACT\r", NULL, 0, QIACT_TIMEOUT_MS) == UMODEM_OK)
    {
      g_data_connected = 1;
      return UMODEM_OK;
    }
    umodem_hal_delay_ms(1000);
  }

  return UMODEM_ERR;
}

static umodem_result_t quectel_m65_sock_deinit()
{
  if (!g_sim_inserted)
    return UMODEM_SIM_NOT_INSERTED;
  if (!g_data_connected)
    return UMODEM_OK;

  // AT+QIDEACT
  if (umodem_at_send("AT+QIDEACT\r", NULL, 0, QIDEACT_TIMEOUT_MS) == UMODEM_OK)
  {
    g_data_connected = 0;
    return UMODEM_OK;
  }

  return UMODEM_ERR;
}

static int quectel_m65_sock_create(umodem_sock_type_t type)
{
  for (int i = 0; i < QUECTEL_M65_MAX_SOCKETS; i++)
  {
    if (g_sockets[i].sockfd == 0)
    {
      g_sockets[i].sockfd = i + 1;
      g_sockets[i].type = type;
      return g_sockets[i].sockfd;
    }
  }
  return -1; // No available sockets
}

static umodem_result_t quectel_m65_sock_connect(int sockfd, const char *host, size_t host_len, uint16_t port, uint32_t timeout_ms)
{
  if (sockfd <= 0 || sockfd > QUECTEL_M65_MAX_SOCKETS)
    return UMODEM_PARAM;

  quectel_m65_sockfd *sock = &g_sockets[sockfd - 1];
  if (sock->sockfd != sockfd)
    return UMODEM_PARAM;

  if (sock->connected)
    return UMODEM_OK; // Already connected
  sock->connected = 0;

  char cmd[128];
  if (sock->type == UMODEM_SOCK_TCP)
  {
    snprintf(cmd, sizeof(cmd), "AT+QIOPEN=%d,\"TCP\",\"%.*s\",%d\r",
             sockfd - 1, (int)host_len, host, port);
  }
  else if (sock->type == UMODEM_SOCK_UDP)
  {
    snprintf(cmd, sizeof(cmd), "AT+QIOPEN=%d,\"UDP\",\"%.*s\",%d\r",
             sockfd - 1, (int)host_len, host, port);
  }
  else
  {
    return UMODEM_PARAM;
  }

  if (umodem_at_send(cmd, NULL, 0, 10000) != UMODEM_OK)
    return UMODEM_ERR;

  // Wait for CONNECT OK if timeout_ms > 0
  if (timeout_ms > 0)
  {
    uint32_t start = umodem_hal_millis();
    while (umodem_hal_millis() - start < timeout_ms)
    {
      umodem_poll();
      if (sock->connected)
        return UMODEM_OK;
      else if (sock->connected == -1)
        return UMODEM_ERR; // Connection failed
      umodem_hal_delay_ms(100);
    }
    return UMODEM_TIMEOUT;
  }
  else
    return UMODEM_OK;

  return UMODEM_TIMEOUT;
}

static umodem_result_t quectel_m65_sock_close(int sockfd)
{
  if (sockfd <= 0 || sockfd > QUECTEL_M65_MAX_SOCKETS)
    return UMODEM_PARAM;

  quectel_m65_sockfd *sock = &g_sockets[sockfd - 1];
  if (sock->sockfd != sockfd)
    return UMODEM_PARAM;

  if (!sock->connected)
    return UMODEM_OK; // Already closed

  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+QICLOSE=%d\r", sockfd - 1);

  umodem_result_t result = umodem_at_send(cmd, NULL, 0, 5000);
  if (result == UMODEM_OK)
  {
    sock->connected = 0; // Mark socket as disconnected
    sock->sockfd = 0;    // Free socket
  }
  return result;
}

static umodem_sock_driver_t quectel_m65_sock_driver = {
    .sock_init = quectel_m65_sock_init,
    .sock_deinit = quectel_m65_sock_deinit,
    .sock_create = quectel_m65_sock_create,
    .sock_connect = quectel_m65_sock_connect,
    .sock_close = quectel_m65_sock_close,
    .sock_send = NULL,
    .sock_recv = NULL,
};

static umodem_driver_t s_quectel_m65_driver = {
    .init = quectel_m65_init,
    .deinit = quectel_m65_deinit,
    .get_imei = quectel_m65_get_imei,
    .get_signal = quectel_m65_get_signal,
    .get_iccid = quectel_m65_get_iccid,
    .handle_urc = quectel_m65_handle_urc,
    .sock_driver = &quectel_m65_sock_driver,
    .http_driver = NULL,
    .mqtt_driver = NULL,
    .ppp_driver = NULL,
    .umodem_initialized = 0,
};

umodem_driver_t *g_umodem_driver = &s_quectel_m65_driver;

#endif