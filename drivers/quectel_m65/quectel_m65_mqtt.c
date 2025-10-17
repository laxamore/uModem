#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "umodem_at.h"
#include "umodem_event.h"
#include "umodem_hal.h"
#include "umodem_utils.h"

#include "quectel_m65.h"
#include "quectel_m65_sock.h"
#include "quectel_m65_mqtt.h"

typedef struct mqtt_message mqtt_message_t;
struct mqtt_message {
  umodem_event_mqtt_data_t* event_data;
  mqtt_message_t* next;
};

static int g_mqtt_initialized = 0;
static mqtt_message_t* g_mqtt_messages = NULL;

quectel_m65_mqtt_conn_t g_mqtt_conns[QUECTEL_M65_MAX_MQTT_CONNS] = {0};

static uint16_t find_mqtt_message_id(
    int sockfd, const char* topic, size_t topic_len) {
  mqtt_message_t* cur = g_mqtt_messages;
  while (cur) {
    if (UMODEM_MEMMEM(cur->event_data->topic, cur->event_data->topic_len, topic,
            topic_len) &&
        cur->event_data->sockfd == sockfd)
      return cur->event_data->id;
    cur = cur->next;
  }
  return 0;
}

static umodem_result_t quectel_m65_mqtt_init(void) {
  if (!g_sim_inserted) return UMODEM_SIM_NOT_INSERTED;

  for (int i = 0; i < QUECTEL_M65_MAX_SOCKETS; i++) {
    if (i < QUECTEL_M65_MAX_MQTT_CONNS) g_mqtt_conns[i].sock = g_sockets[i];
  }

  uint32_t start = umodem_hal_millis();
  while (umodem_hal_millis() - start < NETWORK_ATTACH_TIMEOUT_MS &&
      !g_network_attached) {
    umodem_poll();
    umodem_hal_delay_ms(1000);
  }

  if (!g_network_attached) return UMODEM_ERR;

  g_mqtt_initialized = 1;
  return UMODEM_OK;
}

static umodem_result_t quectel_m65_mqtt_deinit() {
  if (!g_sim_inserted) return UMODEM_SIM_NOT_INSERTED;

  mqtt_message_t* cur = g_mqtt_messages;
  while (cur) {
    mqtt_message_t* next = cur->next;

    if (cur->event_data && cur->event_data->data) free(cur->event_data->data);
    if (cur->event_data) free(cur->event_data);
    free(cur);

    cur = next;
  }

  g_mqtt_initialized = 0;
  return UMODEM_OK;
}

static int quectel_m65_mqtt_connect(
    const char* host, uint16_t port, const umodem_mqtt_connect_opts_t* opts) {
  if (!g_mqtt_initialized) return -1;

  if (!opts->client_id) return -1;

  int connection_index = -1;
  for (int i = 0; i < QUECTEL_M65_MAX_SOCKETS; i++) {
    if (g_mqtt_conns[i].sock.sockfd == 0) {
      g_mqtt_conns[i].sock.sockfd = i + 1;
      g_mqtt_conns[i].sock.type = UMODEM_SOCK_TCP;
      connection_index = i;
      break;
    }
  }

  char cmd[256] = {0};

  if (opts->will) {
    if (!opts->will->topic || !opts->will->message ||
        strlen(opts->will->topic) == 0 || strlen(opts->will->message) == 0 ||
        (opts->will->retain != 0 && opts->will->retain != 1) ||
        (opts->will->qos < UMODEM_MQTT_QOS_0 &&
            opts->will->qos > UMODEM_MQTT_QOS_2))
      return -1;

    snprintf(cmd, sizeof(cmd), "AT+QMTCFG=\"WILL\",%d,1,%d,%d,\"%s\",\"%s\"\r",
        connection_index, opts->will->qos, opts->will->retain,
        opts->will->topic, opts->will->message);
    if (umodem_at_send(cmd, NULL, 0, QMTCFG_TIMEOUT_MS) != UMODEM_OK) return -1;
  }

  snprintf(cmd, sizeof(cmd), "AT+QMTCFG=\"TIMEOUT\",%d,%d,0\r",
      connection_index, opts->delivery_timeout_in_seconds);
  if (umodem_at_send(cmd, NULL, 0, QMTCFG_TIMEOUT_MS) != UMODEM_OK) return -1;

  snprintf(cmd, sizeof(cmd), "AT+QMTCFG=\"SESSION\",%d,%d\r", connection_index,
      opts->disable_clean_session ? 0 : 1);
  if (umodem_at_send(cmd, NULL, 0, QMTCFG_TIMEOUT_MS) != UMODEM_OK) return -1;

  if (opts->keepalive > 3600) return -1;

  snprintf(cmd, sizeof(cmd), "AT+QMTCFG=\"KEEPALIVE\",%d,%d\r",
      connection_index, opts->keepalive);
  if (umodem_at_send(cmd, NULL, 0, QMTCFG_TIMEOUT_MS) != UMODEM_OK) return -1;

  // TODO : MQTT SSL

  snprintf(cmd, sizeof(cmd), "AT+QMTOPEN=%d,\"%s\",%d\r", connection_index,
      host, port);
  if (umodem_at_send(cmd, NULL, 0, 1000) != UMODEM_OK) return -1;

  // Wait QMTOPEN result
  uint32_t start = umodem_hal_millis();
  g_mqtt_conns[connection_index].context_open = 0;
  while (umodem_hal_millis() - start < QMTOPEN_TIMEOUT_MS &&
      g_mqtt_conns[connection_index].context_open == 0) {
    umodem_poll();
    umodem_hal_delay_ms(1000);
  }

  if (g_mqtt_conns[connection_index].context_open <= 0) return -1;

  snprintf(cmd, sizeof(cmd), "AT+QMTCONN=%d,\"%s\",\"%s\",\"%s\"\r",
      connection_index, opts->client_id, !opts->username ? "" : opts->username,
      !opts->password ? "" : opts->password);
  if (umodem_at_send(cmd, NULL, 0, 1000) != UMODEM_OK) return -1;

  // Wait QMTCONN result
  start = umodem_hal_millis();
  g_mqtt_conns[connection_index].sock.connected = 0;
  while (umodem_hal_millis() - start < QMTCONN_TIMEOUT_MS &&
      g_mqtt_conns[connection_index].sock.connected == 0) {
    umodem_poll();
    umodem_hal_delay_ms(1000);
  }

  if (g_mqtt_conns[connection_index].sock.connected <= 0) return -1;
  return connection_index + 1;
}

static umodem_result_t quectel_m65_mqtt_disconnect(int sockfd) {
  if (!g_mqtt_initialized || sockfd <= 0 || sockfd > 6 ||
      !g_mqtt_conns[sockfd - 1].sock.connected)
    return UMODEM_ERR;

  char cmd[16];
  snprintf(cmd, sizeof(cmd), "AT+QMTDISC=%d\r", sockfd - 1);
  if (umodem_at_send(cmd, NULL, 0, 1000) != UMODEM_OK) return UMODEM_ERR;

  g_mqtt_conns[sockfd - 1].sock.connected = 0;
  return UMODEM_OK;
}

static umodem_result_t quectel_m65_mqtt_publish(int sockfd, const char* topic,
    size_t topic_len, const void* payload, size_t len, umodem_mqtt_qos_t qos,
    int retain) {
  if (!g_mqtt_initialized || sockfd <= 0 || sockfd > 6 ||
      !g_mqtt_conns[sockfd - 1].sock.connected || !topic || topic_len <= 0 ||
      !payload || len <= 0)
    return UMODEM_ERR;

  int cmd_size = 128;
  char* cmd = (char*)calloc(cmd_size, 1);
  if (!cmd) return UMODEM_ERR;

  uint16_t id = mqtt_add_message(sockfd, topic, topic_len, payload, len, 0);

  snprintf(cmd, cmd_size, "AT+QMTPUB=%d,%d,%d,%d,\"%s\"\r", sockfd - 1, id, qos,
      retain, topic);
  if (umodem_at_send(cmd, NULL, 0, 1000) != UMODEM_OK) {
    free(cmd);
    return UMODEM_ERR;
  }

  if (umodem_hal_send(payload, len) > 0) {
    char ctrl_z[2] = {0x1a, 0x0};
    free(cmd);
    return umodem_at_send(ctrl_z, NULL, 0, 1000);
  }

  free(cmd);
  return UMODEM_ERR;
}

static umodem_result_t quectel_m65_mqtt_subscribe(
    int sockfd, const char* topic, size_t topic_len, umodem_mqtt_qos_t qos) {
  umodem_result_t ret = UMODEM_ERR;

  if (!g_mqtt_initialized || sockfd <= 0 || sockfd > 6 ||
      !g_mqtt_conns[sockfd - 1].sock.connected || !topic || topic_len <= 0 ||
      qos > UMODEM_MQTT_QOS_2 || qos < 0)
    return ret;

  uint16_t id = mqtt_add_message(sockfd, topic, topic_len, NULL, 0, 1);
  char cmd[128] = {0};
  snprintf(cmd, sizeof(cmd), "AT+QMTSUB=%d,%d,\"%s\",%d\r", sockfd - 1, id,
      topic, qos);
  if (umodem_at_send(cmd, NULL, 0, 1000) == UMODEM_OK) ret = UMODEM_OK;

  return ret;
}

static umodem_result_t quectel_m65_mqtt_unsubscribe(
    int sockfd, const char* topic, size_t topic_len) {
  umodem_result_t ret = UMODEM_ERR;
  if (!g_mqtt_initialized || sockfd <= 0 || sockfd > 6 ||
      !g_mqtt_conns[sockfd - 1].sock.connected || !topic || topic_len <= 0)
    return ret;

  uint16_t id = find_mqtt_message_id(sockfd, topic, topic_len);
  if (!id) return ret;

  char cmd[128] = {0};
  snprintf(cmd, sizeof(cmd), "AT+QMTUNS=%d,%d,\"%s\"\r", sockfd - 1, id, topic);
  if (umodem_at_send(cmd, NULL, 0, 1000) == UMODEM_OK) ret = UMODEM_OK;

  umodem_event_mqtt_data_t* event_data = mqtt_pop_message(id);
  if (event_data && event_data->data) free(event_data->data);
  if (event_data) free(event_data);

  return ret;
}

uint16_t mqtt_add_message(uint8_t sockfd, const char* topic, size_t topic_len,
    const uint8_t* payload, size_t len, uint8_t type) {
  static uint16_t id = 0;

  umodem_event_mqtt_data_t* mqtt_event_data =
      calloc(sizeof(umodem_event_mqtt_data_t), 1);
  if (!mqtt_event_data) return 0;

  mqtt_event_data->sockfd = sockfd;
  mqtt_event_data->topic = topic;
  mqtt_event_data->topic_len = topic_len;
  mqtt_event_data->data_len = len;

  uint8_t* msg_payload = NULL;
  if (payload && len > 0) {
    msg_payload = calloc(len, 1);
    if (!msg_payload) {
      free(mqtt_event_data);
      return 0;
    }

    memcpy(msg_payload, payload, len);
    mqtt_event_data->data = msg_payload;
  }

  mqtt_message_t* mqtt_message = calloc(sizeof(mqtt_message_t), 1);
  if (!mqtt_message) {
    free(mqtt_event_data);
    if (msg_payload) free(msg_payload);
    return 0;
  }
  mqtt_message->next = NULL;
  mqtt_message->event_data = mqtt_event_data;

  id = id == 65535 ? 1 : id + 1;
  if (!g_mqtt_messages) {
    mqtt_event_data->id = id;
    g_mqtt_messages = mqtt_message;
  } else {
    mqtt_event_data->id = id;
    g_mqtt_messages->next = mqtt_message;
  }

  return id;
}

umodem_event_mqtt_data_t* mqtt_pop_message(uint16_t id) {
  umodem_event_mqtt_data_t* event_data = NULL;

  mqtt_message_t* prev = NULL;
  mqtt_message_t* cur = g_mqtt_messages;
  while (cur) {
    if (cur->event_data->id == id) {
      event_data = cur->event_data;

      if (prev)
        prev->next = cur->next;
      else
        g_mqtt_messages = cur->next;

      free(cur);
      break;
    }

    prev = cur;
    cur = cur->next;
  }

  return event_data;
}

umodem_event_mqtt_data_t* mqtt_get_message(uint16_t id) {
  umodem_event_mqtt_data_t* event_data = NULL;

  mqtt_message_t* cur = g_mqtt_messages;
  while (cur) {
    if (cur->event_data->id == id) return cur->event_data;
    cur = cur->next;
  }

  return event_data;
}

umodem_event_mqtt_data_t* mqtt_get_message_by_topic(
    const char* topic, size_t topic_len) {
  umodem_event_mqtt_data_t* event_data = NULL;

  mqtt_message_t* cur = g_mqtt_messages;
  while (cur) {
    if (UMODEM_MEMMEM(cur->event_data->topic, cur->event_data->topic_len, topic,
            topic_len))
      return cur->event_data;
    cur = cur->next;
  }

  return event_data;
}

void umodem_event_mqtt_pub_dtor(umodem_event_t* self) {
  umodem_event_mqtt_data_t* event_data = (umodem_event_mqtt_data_t*)self->data;
  free(event_data->data);
  free(event_data);
}

void umodem_event_mqtt_sub_dtor(umodem_event_t* self) {
  umodem_event_mqtt_data_t* event_data = (umodem_event_mqtt_data_t*)self->data;
  free(event_data->data);
  event_data->data = NULL;
}

const umodem_mqtt_driver_t quectel_m65_mqtt_driver = {
    .mqtt_init = quectel_m65_mqtt_init,
    .mqtt_deinit = quectel_m65_mqtt_deinit,
    .mqtt_connect = quectel_m65_mqtt_connect,
    .mqtt_disconnect = quectel_m65_mqtt_disconnect,
    .mqtt_publish = quectel_m65_mqtt_publish,
    .mqtt_subscribe = quectel_m65_mqtt_subscribe,
    .mqtt_unsubscribe = quectel_m65_mqtt_unsubscribe,
};
