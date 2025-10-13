#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "umodem_config.h"
#include "umodem_driver.h"
#include "umodem_hal.h"
#include "umodem_at.h"

#include "umodem_mqtt.h"
#include "umodem_sock.h"

#if defined(UMODEM_MODEM_QUECTEL_M65)

#define QIREGAPP_TIMEOUT_MS (1000 * 5)        // 5 seconds
#define QIACT_TIMEOUT_MS (1000 * 150)         // 150 seconds
#define QIDEACT_TIMEOUT_MS (1000 * 40)        // 40 seconds
#define NETWORK_ATTACH_TIMEOUT_MS (1000 * 90) // 90 seconds
//
#define QMTCFG_TIMEOUT_MS (1000 * 5)   // 5 seconds
#define QMTOPEN_TIMEOUT_MS (1000 * 75) // 5 seconds
#define QMTCONN_TIMEOUT_MS (1000 * 60) // 5 seconds

#define QUECTEL_M65_MAX_SOCKETS 6

typedef struct
{
  int sockfd;
  umodem_sock_type_t type;
  int connection_opened; // used in mqtt
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
  if (umodem_at_send("AT+CPIN?\r", response, sizeof(response), 2000) !=
      UMODEM_OK)
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

static umodem_event_info_t quectel_m65_handle_urc(const uint8_t *buf, size_t len)
{
  char dup_buf[len + 1];
  memcpy(dup_buf, buf, len);
  dup_buf[len] = '\0';

  if (strstr(dup_buf, "+PDP DEACT"))
  {
    g_data_connected = 0;
    return (umodem_event_info_t){.event = UMODEM_EVENT_DATA_DOWN, .event_data = NULL};
  }
  else if (strstr(dup_buf, "+CREG:"))
  {
    // If buf containing ',' then it's not URC +CREG: <stat>
    if (strchr(dup_buf, ','))
      return (umodem_event_info_t){.event = UMODEM_URC_IGNORE, .event_data = NULL};

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
    return (umodem_event_info_t){.event = UMODEM_URC_IGNORE, .event_data = NULL};
  }
  else if (strstr(dup_buf, "+CMTI:"))
    return (umodem_event_info_t){.event = UMODEM_EVENT_SMS_RECEIVED, .event_data = NULL}; /* SMS received */
  else if (strstr(dup_buf, "CONNECT OK"))
  {
    // <index>, CONNECT OK
    char *token = strtok(dup_buf, ",");
    if (token)
    {
      int index = atoi(token);
      if (index >= 0 && index < QUECTEL_M65_MAX_SOCKETS)
      {
        g_sockets[index].connected = 1; // Mark socket as connected
        return (umodem_event_info_t){.event = UMODEM_EVENT_SOCK_CONNECTED, .event_data = &g_sockets[index].sockfd};
      }
    }
    return (umodem_event_info_t){.event = UMODEM_URC_IGNORE, .event_data = NULL};
  }
  else if (strstr(dup_buf, "CONNECT FAIL"))
  {
    // <index>, CONNECT FAIL
    char *token = strtok(dup_buf, ",");
    if (token)
    {
      int index = atoi(token);
      if (index >= 0 && index < QUECTEL_M65_MAX_SOCKETS)
        g_sockets[index].connected = -1; // Mark socket as failed
    }
    return (umodem_event_info_t){.event = UMODEM_URC_IGNORE, .event_data = NULL};
  }
  else if (strstr(dup_buf, "CLOSED"))
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
        return (umodem_event_info_t){.event = UMODEM_EVENT_SOCK_CLOSED, .event_data = &g_sockets[index].sockfd};
      }
    }
    return (umodem_event_info_t){.event = UMODEM_URC_IGNORE, .event_data = NULL};
  }
  else if (strstr((const char*)buf, "+QIRDI:"))
  {
    // +QIRDI: 0,1,<index>
    char *token = strtok(dup_buf, ":");
    if (token)
      token = strtok(NULL, ","); // skip 0
    if (token)
      token = strtok(NULL, ","); // skip 1
    if (token)
      token = strtok(NULL, "\r"); // get <index>
    if (token)
    {
      int index = atoi(token);
      if (index >= 0 && index < QUECTEL_M65_MAX_SOCKETS)
        return (umodem_event_info_t){.event = UMODEM_EVENT_SOCK_DATA_RECEIVED, .event_data = &g_sockets[index].sockfd};
    }
    return (umodem_event_info_t){.event = UMODEM_URC_IGNORE, .event_data = NULL};
  }
  else if (strstr((const char *)buf, "+QMTOPEN:"))
  {
    // +QMTOPEN: <index>,result
    char *token = strtok(dup_buf, ":");
    if (token)
      token = strtok(NULL, ","); // index token
    if (token)
    {
      int index = atoi(token);
      if (index >= 0 && index < QUECTEL_M65_MAX_SOCKETS)
      {
        token = strtok(NULL, ","); // result token
        if (token)
        {
          int result = atoi(token);
          if (result == 0)
            g_sockets[index].connection_opened = 1;
        }
      }
    }
    return (umodem_event_info_t){.event = UMODEM_URC_IGNORE, .event_data = NULL};
  }
  else if (strstr((const char *)buf, "+QMTCONN:"))
  {
    // +QMTCONN: <index>,<result>,<retcode>
    char *token = strtok(dup_buf, ":");
    if (token)
      token = strtok(NULL, ","); // index token
    if (token)
    {
      int index = atoi(token);
      if (index >= 0 && index < QUECTEL_M65_MAX_SOCKETS)
      {
        token = strtok(NULL, ","); // result token
        if (token)
        {
          int result = atoi(token);
          if (result == 0)
          {
            token = strtok(NULL, ","); // result token
            if (token)
            {
              int retcode = atoi(token);
              if (retcode == 0)
              {
                g_sockets[index].connected = 1;
                return (umodem_event_info_t){
                    .event = UMODEM_EVENT_SOCK_CONNECTED,
                    .event_data = &g_sockets[index].sockfd};
              }
            }
          }
        }
      }
    }
    return (umodem_event_info_t){.event = UMODEM_URC_IGNORE, .event_data = NULL};
  }
  else if (strstr((const char *)buf, "+QMTPUB:"))
  {
    // +QMTPUB: <index>,<msg_id>,<result>
    char *token = strtok(dup_buf, ":");
    return (umodem_event_info_t){.event = UMODEM_URC_IGNORE, .event_data = NULL};
  }

  return (umodem_event_info_t){.event = UMODEM_URC_IGNORE, .event_data = NULL};
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

// {{ Quectel M65 Sock Driver

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
           g_umodem_driver->apn->apn, g_umodem_driver->apn->user, g_umodem_driver->apn->pass);

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

static int quectel_m65_sock_send(int sockfd, const uint8_t *data, size_t len)
{
  if (sockfd <= 0 || sockfd > QUECTEL_M65_MAX_SOCKETS || !data || len == 0)
    return -1;

  quectel_m65_sockfd *sock = &g_sockets[sockfd - 1];
  if (sock->sockfd != sockfd || !sock->connected)
    return -1;

  char cmd[40];
  snprintf(cmd, sizeof(cmd), "AT+QISEND=%d,%d\r", sockfd - 1, (int)len);

  // Send command
  if (umodem_at_send(cmd, NULL, 0, 5000) != UMODEM_OK)
    return -1;

  if (umodem_at_send((const char *)data, NULL, 0, 10000) == UMODEM_OK)
    return len;

  return -1; // timeout or fail
}

static int quectel_m65_sock_recv(int sockfd, uint8_t *buf, size_t len)
{
  if (sockfd <= 0 || sockfd > QUECTEL_M65_MAX_SOCKETS || !buf || len == 0)
    return -1;

  quectel_m65_sockfd *sock = &g_sockets[sockfd - 1];
  if (sock->sockfd != sockfd || !sock->connected)
    return -1;

  // Cap max read to avoid huge allocations
  const size_t MAX_RECV = 1500; // typical MTU
  size_t read_len = (len > MAX_RECV) ? MAX_RECV : len;

  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+QIRD=0,1,%d,%d\r", sockfd - 1, (int)read_len);

  // Allocate response buffer on heap
  char *response = (char *)malloc(UMODEM_RX_BUF_SIZE);
  if (!response)
    return -1; // out of memory

  int result = -1;
  if (umodem_at_send(cmd, response, UMODEM_RX_BUF_SIZE, 5000) == UMODEM_OK)
  {
    int data_len = 0;
    char *qird = strstr(response, "+QIRD:");
    if (qird)
    {
      // Example response: +QIRD: 116.228.146.250:7070,TCP,36
      // Skip to after "+QIRD:"
      qird += 6;

      // Find the first comma (end of quoted address)
      char *p = strchr(qird, ',');
      if (p)
      {
        // Find the second comma (end of protocol)
        p = strchr(p + 1, ',');
        if (p)
        {
          // Move to the number after second comma
          p++;
          data_len = atoi(p);
        }
      }
    }

    if (data_len > 0)
    {
      char *data_start = strstr(response, "\r\n");
      if (data_start)
      {
        data_start += 2;
        if (data_len > 0)
        {
          size_t copy_len = (size_t)data_len;
          if (copy_len > len)
            copy_len = len;

          memcpy(buf, data_start, copy_len);
          result = (int)copy_len;
        }
        else
          result = 0; // no data
      }
    }
    else
      result = 0;
  }

  free(response);
  return result;
}

static umodem_sock_driver_t quectel_m65_sock_driver = {
    .sock_init = quectel_m65_sock_init,
    .sock_deinit = quectel_m65_sock_deinit,
    .sock_create = quectel_m65_sock_create,
    .sock_connect = quectel_m65_sock_connect,
    .sock_close = quectel_m65_sock_close,
    .sock_send = quectel_m65_sock_send,
    .sock_recv = quectel_m65_sock_recv,
};

// End Quectel M65 Sock Driver }}

// {{ Quectel M65 MQTT Driver

static int g_mqtt_initialized = 0;

umodem_result_t quectel_m65_mqtt_init(void)
{
  if (!g_sim_inserted)
    return UMODEM_SIM_NOT_INSERTED;

  uint32_t start = umodem_hal_millis();
  while (umodem_hal_millis() - start < NETWORK_ATTACH_TIMEOUT_MS &&
         !g_network_attached)
  {
    umodem_poll();
    umodem_hal_delay_ms(1000);
  }

  if (!g_network_attached)
    return UMODEM_ERR;

  // Doesn't need to initialize anything all PDP context handled by QMTOPEN

  g_mqtt_initialized = 1;
  return UMODEM_OK;
}

static umodem_result_t quectel_m65_mqtt_deinit()
{
  if (!g_sim_inserted)
    return UMODEM_SIM_NOT_INSERTED;

  g_mqtt_initialized = 0;
  return UMODEM_OK;
}

static int quectel_m65_mqtt_connect(const char *host, uint16_t port,
                                    const umodem_mqtt_connect_opts_t *opts)
{
  if (!g_mqtt_initialized)
    return -1;

  if (!opts->client_id)
    return -1;

  int connection_index = -1;
  for (int i = 0; i < QUECTEL_M65_MAX_SOCKETS; i++)
  {
    if (g_sockets[i].sockfd == 0)
    {
      g_sockets[i].sockfd = i + 1;
      g_sockets[i].type = UMODEM_SOCK_TCP;
      connection_index = i;
      break;
    }
  }

  int cmd_size = 256;
  char *cmd = (char *)calloc(cmd_size, 1);
  if (cmd == NULL)
    return -1;

  if (opts->will)
  {
    if (!opts->will->topic || !opts->will->message ||
        strlen(opts->will->topic) == 0 || strlen(opts->will->message) == 0 ||
        (opts->will->retain != 0 && opts->will->retain != 1) ||
        (opts->will->qos < UMODEM_MQTT_QOS_0 &&
         opts->will->qos > UMODEM_MQTT_QOS_2))
      return -1;

    snprintf(cmd, cmd_size, "AT+QMTCFG=\"WILL\",%d,1,%d,%d,\"%s\",\"%s\"\r",
             connection_index, opts->will->qos, opts->will->retain,
             opts->will->topic, opts->will->message);
    if (umodem_at_send(cmd, NULL, 0, QMTCFG_TIMEOUT_MS) != UMODEM_OK)
    {
      free(cmd);
      return -1;
    }
  }

  snprintf(cmd, cmd_size, "AT+QMTCFG=\"TIMEOUT\",%d,%d,0\r", connection_index,
           opts->delivery_timeout_in_seconds);
  if (umodem_at_send(cmd, NULL, 0, QMTCFG_TIMEOUT_MS) != UMODEM_OK)
  {
    free(cmd);
    return -1;
  }

  snprintf(cmd, cmd_size, "AT+QMTCFG=\"SESSION\",%d,%d\r", connection_index,
           opts->disable_clean_session ? 0 : 1);
  if (umodem_at_send(cmd, NULL, 0, QMTCFG_TIMEOUT_MS) != UMODEM_OK)
  {
    free(cmd);
    return -1;
  }

  if (opts->keepalive > 3600)
  {
    free(cmd);
    return -1;
  }

  snprintf(cmd, cmd_size, "AT+QMTCFG=\"KEEPALIVE\",%d,%d\r", connection_index,
           opts->keepalive);
  if (umodem_at_send(cmd, NULL, 0, QMTCFG_TIMEOUT_MS) != UMODEM_OK)
  {
    free(cmd);
    return -1;
  }

  // TODO : MQTT SSL

  snprintf(cmd, cmd_size, "AT+QMTOPEN=%d,\"%s\",%d\r", connection_index, host,
           port);
  if (umodem_at_send(cmd, NULL, 0, 1000) != UMODEM_OK)
  {
    free(cmd);
    return -1;
  }

  // Wait QMTOPEN result
  uint32_t start = umodem_hal_millis();
  while (umodem_hal_millis() - start < QMTOPEN_TIMEOUT_MS &&
         !g_sockets[connection_index].connection_opened)
  {
    umodem_poll();
    umodem_hal_delay_ms(1000);
  }

  snprintf(cmd, cmd_size, "AT+QMTCONN=%d,\"%s\",\"%s\",\"%s\"\r",
           connection_index, opts->client_id,
           !opts->username ? "" : opts->username,
           !opts->password ? "" : opts->password);
  if (umodem_at_send(cmd, NULL, 0, 1000) != UMODEM_OK)
  {
    free(cmd);
    return -1;
  }

  // Wait QMTCONN result
  start = umodem_hal_millis();
  while (umodem_hal_millis() - start < QMTCONN_TIMEOUT_MS &&
         !g_sockets[connection_index].connected)
  {
    umodem_poll();
    umodem_hal_delay_ms(1000);
  }

  free(cmd);
  return connection_index + 1;
}

static umodem_result_t quectel_m65_mqtt_disconnect(int sockfd)
{
  if (!g_mqtt_initialized || sockfd <= 0 || sockfd > 6 ||
      !g_sockets[sockfd - 1].connected)
    return UMODEM_ERR;

  char cmd[16];
  snprintf(cmd, sizeof(cmd), "AT+QMTDISC=%d\r", sockfd - 1);
  if (umodem_at_send(cmd, NULL, 0, 1000) != UMODEM_OK)
    return UMODEM_ERR;

  g_sockets[sockfd - 1].connected = 0;
  return UMODEM_OK;
}

static umodem_result_t quectel_m65_mqtt_publish(int sockfd, const char *topic,
                                                const void *payload, size_t len,
                                                umodem_mqtt_qos_t qos,
                                                int retain)
{
  if (!g_mqtt_initialized || sockfd <= 0 || sockfd > 6 ||
      !g_sockets[sockfd - 1].connected || !topic || !payload || len <= 0)
    return UMODEM_ERR;

  int cmd_size = 128;
  char *cmd = (char *)calloc(cmd_size, 1);
  if (!cmd)
    return UMODEM_ERR;

  snprintf(cmd, cmd_size, "AT+QMTPUB=%d,1,%d,%d,\"%s\"\r", sockfd - 1, qos,
           retain, topic);
  if (umodem_at_send(cmd, NULL, 0, 1000) != UMODEM_OK)
  {
    free(cmd);
    return UMODEM_ERR;
  }

  if (umodem_hal_send(payload, len) > 0)
  {
    char ctrl_z[2] = {0x1a, 0x0};
    free(cmd);
    return umodem_at_send(ctrl_z, NULL, 0, 1000);
  }

  free(cmd);
  return UMODEM_ERR;
}

static umodem_mqtt_driver_t quectel_m65_mqtt_driver = {
    .mqtt_init = quectel_m65_mqtt_init,
    .mqtt_deinit = quectel_m65_mqtt_deinit,
    .mqtt_connect = quectel_m65_mqtt_connect,
    .mqtt_disconnect = quectel_m65_mqtt_disconnect,
    .mqtt_publish = quectel_m65_mqtt_publish,
    .mqtt_subscribe = NULL,
    .mqtt_unsubscribe = NULL,
};

// End Quectel M65 MQTT Driver }}

static umodem_driver_t s_quectel_m65_driver = {
    .init = quectel_m65_init,
    .deinit = quectel_m65_deinit,
    .get_imei = quectel_m65_get_imei,
    .get_signal = quectel_m65_get_signal,
    .get_iccid = quectel_m65_get_iccid,
    .handle_urc = quectel_m65_handle_urc,
    .sock_driver = &quectel_m65_sock_driver,
    .http_driver = NULL,
    .mqtt_driver = &quectel_m65_mqtt_driver,
    .ppp_driver = NULL,
    .umodem_initialized = 0,
};

umodem_driver_t *g_umodem_driver = &s_quectel_m65_driver;

#endif