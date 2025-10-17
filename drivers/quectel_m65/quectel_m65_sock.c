#include <stdio.h>
#include <string.h>

#include "quectel_m65.h"
#include "quectel_m65_sock.h"

#include "umodem_at.h"
#include "umodem_hal.h"
#include "umodem_utils.h"

#define QIRD_RESPONSE_BUF_SIZE (64 + QIRD_MAX_RECV_LEN)

quectel_m65_socket_t g_sockets[QUECTEL_M65_MAX_SOCKETS] = {0};

static int is_valid_hostname(const char* host, size_t len) {
  if (!host || len == 0) return 0;
  for (size_t i = 0; i < len; i++) {
    char c = host[i];
    if (c == '"' || c == '\r' || c == '\n' || c == ',')
      return 0; // unsafe for AT command
  }
  return 1;
}

static umodem_result_t quectel_m65_sock_init(void) {
  if (!g_sim_inserted) return UMODEM_SIM_NOT_INSERTED;

  uint32_t start = umodem_hal_millis();
  while (umodem_hal_millis() - start < NETWORK_ATTACH_TIMEOUT_MS &&
      !g_network_attached) {
    umodem_poll();
    umodem_hal_delay_ms(1000);
  }
  if (!g_network_attached) return UMODEM_ERR;

  if (umodem_at_send("AT+QIMUX=1\r", NULL, 0, QIMUX_TIMEOUT_MS) != UMODEM_OK)
    return UMODEM_ERR;

  if (umodem_at_send("AT+QINDI=1\r", NULL, 0, QINDI_TIMEOUT_MS) != UMODEM_OK)
    return UMODEM_ERR;

  char cmd[128];
  int written = snprintf(cmd, sizeof(cmd), "AT+QIREGAPP=\"%s\",\"%s\",\"%s\"\r",
      g_umodem_driver->apn->apn, g_umodem_driver->apn->user,
      g_umodem_driver->apn->pass);
  if (written < 0 || written >= (int)sizeof(cmd)) return UMODEM_PARAM;

  start = umodem_hal_millis();
  while (umodem_hal_millis() - start < QIREGAPP_TIMEOUT_MS) {
    if (umodem_at_send(cmd, NULL, 0, QIREGAPP_TIMEOUT_MS) == UMODEM_OK) break;
    umodem_hal_delay_ms(1000);
  }

  start = umodem_hal_millis();
  while (umodem_hal_millis() - start < QIACT_TIMEOUT_MS) {
    if (umodem_at_send("AT+QIACT\r", NULL, 0, QIACT_TIMEOUT_MS) == UMODEM_OK) {
      g_data_connected = 1;
      return UMODEM_OK;
    }
    umodem_hal_delay_ms(1000);
  }

  return UMODEM_ERR;
}

static umodem_result_t quectel_m65_sock_deinit(void) {
  if (!g_sim_inserted) return UMODEM_SIM_NOT_INSERTED;
  if (!g_data_connected) return UMODEM_OK;

  if (umodem_at_send("AT+QIDEACT\r", NULL, 0, QIDEACT_TIMEOUT_MS) ==
      UMODEM_OK) {
    g_data_connected = 0;
    return UMODEM_OK;
  }
  return UMODEM_ERR;
}

static int quectel_m65_sock_create(umodem_sock_type_t type) {
  if (type != UMODEM_SOCK_TCP && type != UMODEM_SOCK_UDP) return -1;
  for (int i = 0; i < QUECTEL_M65_MAX_SOCKETS; i++) {
    if (g_sockets[i].sockfd == 0) {
      g_sockets[i].sockfd = i + 1;
      g_sockets[i].type = type;
      g_sockets[i].connected = 0;
      return g_sockets[i].sockfd;
    }
  }
  return -1; // No available sockets
}

static umodem_result_t quectel_m65_sock_connect(int sockfd, const char* host,
    size_t host_len, uint16_t port, uint32_t timeout_ms) {
  if (sockfd <= 0 || sockfd > QUECTEL_M65_MAX_SOCKETS || !host || host_len == 0)
    return UMODEM_PARAM;

  if (!is_valid_hostname(host, host_len)) return UMODEM_PARAM;

  quectel_m65_socket_t* sock = &g_sockets[sockfd - 1];
  if (sock->sockfd != sockfd) return UMODEM_PARAM;
  if (sock->connected == 1) return UMODEM_OK; // Already connected

  sock->connected = 0; // Reset state

  char cmd[128];
  const char* proto = (sock->type == UMODEM_SOCK_TCP) ? "TCP" : "UDP";
  int written = snprintf(cmd, sizeof(cmd), "AT+QIOPEN=%d,\"%s\",\"%.*s\",%d\r",
      sockfd - 1, proto, (int)host_len, host, port);
  if (written < 0 || written >= (int)sizeof(cmd)) return UMODEM_PARAM;

  if (umodem_at_send(cmd, NULL, 0, QIOPEN_TIMEOUT_MS) != UMODEM_OK)
    return UMODEM_ERR;

  if (timeout_ms == 0) return UMODEM_OK;

  uint32_t start = umodem_hal_millis();
  while (umodem_hal_millis() - start < timeout_ms) {
    umodem_poll();
    if (sock->connected == 1)
      return UMODEM_OK;
    else if (sock->connected == -1)
      return UMODEM_ERR;
    umodem_hal_delay_ms(100);
  }
  return UMODEM_TIMEOUT;
}

static umodem_result_t quectel_m65_sock_close(int sockfd) {
  if (sockfd <= 0 || sockfd > QUECTEL_M65_MAX_SOCKETS) return UMODEM_PARAM;

  quectel_m65_socket_t* sock = &g_sockets[sockfd - 1];
  if (sock->sockfd != sockfd) return UMODEM_PARAM;
  if (sock->connected == 0) return UMODEM_OK; // Already closed

  char cmd[32];
  int written = snprintf(cmd, sizeof(cmd), "AT+QICLOSE=%d\r", sockfd - 1);
  if (written < 0 || written >= (int)sizeof(cmd)) return UMODEM_PARAM;

  umodem_result_t result = umodem_at_send(cmd, NULL, 0, QICLOSE_TIMEOUT_MS);
  if (result == UMODEM_OK) {
    sock->connected = 0;
    sock->sockfd = 0;
  }
  return result;
}

static int quectel_m65_sock_send(int sockfd, const uint8_t* data, size_t len) {
  if (sockfd <= 0 || sockfd > QUECTEL_M65_MAX_SOCKETS || !data || len == 0)
    return -1;

  quectel_m65_socket_t* sock = &g_sockets[sockfd - 1];
  if (sock->sockfd != sockfd || sock->connected != 1) return -1;

  if (len > QISEND_MAX_SEND_LEN) return -1;

  char cmd[40];
  int written =
      snprintf(cmd, sizeof(cmd), "AT+QISEND=%d,%zu\r", sockfd - 1, len);
  if (written < 0 || written >= (int)sizeof(cmd)) return -1;

  if (umodem_at_send(cmd, NULL, 0, QISEND_CMD_TIMEOUT_MS) != UMODEM_OK)
    return -1;
  if (umodem_at_send((const char*)data, NULL, 0, QISEND_DATA_TIMEOUT_MS) ==
      UMODEM_OK)
    return (int)len;

  return -1;
}

static int quectel_m65_sock_recv(int sockfd, uint8_t* buf, size_t len) {
  if (sockfd <= 0 || sockfd > QUECTEL_M65_MAX_SOCKETS || !buf || len == 0)
    return -1;

  quectel_m65_socket_t* sock = &g_sockets[sockfd - 1];
  if (sock->sockfd != sockfd || sock->connected != 1) return -1;

  size_t read_len = (len > QIRD_MAX_RECV_LEN) ? QIRD_MAX_RECV_LEN : len;

  char cmd[32];
  int written =
      snprintf(cmd, sizeof(cmd), "AT+QIRD=0,1,%d,%zu\r", sockfd - 1, read_len);
  if (written < 0 || written >= (int)sizeof(cmd)) return -1;

  char response[QIRD_RESPONSE_BUF_SIZE] = {0};
  if (umodem_at_send(cmd, response, sizeof(response), QIRD_TIMEOUT_MS) !=
      UMODEM_OK)
    return -1;

  // Parse: +QIRD: <remote>,<proto>,<data_len>
  char* qird = UMODEM_MEMMEM(response, sizeof(response), "+QIRD:", 6);
  if (!qird) return 0;

  size_t qird_len = response + QIRD_RESPONSE_BUF_SIZE - qird;
  char* comma = memchr(qird, ',', qird_len);
  if (!comma) return 0;

  size_t remaining = response + QIRD_RESPONSE_BUF_SIZE - (comma + 1);
  comma = memchr(comma + 1, ',', remaining);
  if (!comma) return 0;

  int data_len = 0;
  if (!UMODEM_STRTOI(comma + 1, 0, (int)QIRD_MAX_RECV_LEN, &data_len) ||
      data_len <= 0)
    return 0;

  // Find start of data: first \r\n after header
  char* header_end = UMODEM_MEMMEM(qird, qird_len, "\r\n", 2);
  if (!header_end) return 0;

  uint8_t* data_start = (uint8_t*)(header_end + 2); // skip \r\n
  if ((uintptr_t)data_start + data_len >
      (uintptr_t)response + QIRD_RESPONSE_BUF_SIZE)
    return -1;

  size_t copy_len = (data_len < len) ? (size_t)data_len : len;
  memcpy(buf, data_start, copy_len);
  return (int)copy_len;
}

const umodem_sock_driver_t quectel_m65_sock_driver = {
    .sock_init = quectel_m65_sock_init,
    .sock_deinit = quectel_m65_sock_deinit,
    .sock_create = quectel_m65_sock_create,
    .sock_connect = quectel_m65_sock_connect,
    .sock_close = quectel_m65_sock_close,
    .sock_send = quectel_m65_sock_send,
    .sock_recv = quectel_m65_sock_recv,
};