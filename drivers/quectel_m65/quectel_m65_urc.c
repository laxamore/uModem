#include <string.h>

#include "quectel_m65.h"
#include "quectel_m65_urc.h"
#include "quectel_m65_sock.h"
#include "quectel_m65_mqtt.h"

#include "umodem_event.h"
#include "umodem_utils.h"

static umodem_event_t quectel_m65_handle_pdp_deact(
    const char* buf, size_t len) {
  g_data_connected = 0;
  return (umodem_event_t){0};
}

static umodem_event_t quectel_m65_handle_creg(const char* buf, size_t len) {
  // Expect: "+CREG: <stat>"
  const char* space = memchr(buf, ' ', len);
  if (!space) return (umodem_event_t){0};
  int stat;
  if (!UMODEM_STRTOI(space + 1, 0, INT_MAX, &stat)) return (umodem_event_t){0};
  if (stat >= 0) { g_network_attached = (stat == 1 || stat == 5) ? 1 : 0; }
  return (umodem_event_t){0};
}

static umodem_event_t quectel_m65_handle_connect_ok(
    const char* buf, size_t len) {
  // Expect: "<sockfd>, CONNECT OK"
  int sockfd;
  if (!UMODEM_STRTOI(buf, 0, QUECTEL_M65_MAX_SOCKETS - 1, &sockfd))
    return (umodem_event_t){0};
  if (sockfd < 0 || sockfd >= QUECTEL_M65_MAX_SOCKETS)
    return (umodem_event_t){0};
  g_sockets[sockfd].connected = 1;
  return (umodem_event_t){.event_flag = UMODEM_EVENT_SOCK_CONNECTED,
      .data = &g_sockets[sockfd].sockfd,
      .dtor = NULL};
}

static umodem_event_t quectel_m65_handle_connect_fail(
    const char* buf, size_t len) {
  // Expect: "<sockfd>, CONNECT FAIL"
  int sockfd;
  if (!UMODEM_STRTOI(buf, 0, QUECTEL_M65_MAX_SOCKETS - 1, &sockfd))
    return (umodem_event_t){0};
  if (sockfd >= 0 && sockfd < QUECTEL_M65_MAX_SOCKETS)
    g_sockets[sockfd].connected = -1;
  return (umodem_event_t){0};
}

static umodem_event_t quectel_m65_handle_closed(const char* buf, size_t len) {
  // Expect: "<sockfd>, CLOSED"
  static int closed_sockfd = 0;
  if (!UMODEM_STRTOI(buf, 0, QUECTEL_M65_MAX_SOCKETS - 1, &closed_sockfd))
    return (umodem_event_t){0};
  if (closed_sockfd < 0 || closed_sockfd >= QUECTEL_M65_MAX_SOCKETS)
    return (umodem_event_t){0};
  g_sockets[closed_sockfd].connected = 0;
  g_sockets[closed_sockfd].sockfd = 0;
  return (umodem_event_t){.event_flag = UMODEM_EVENT_SOCK_CLOSED,
      .data = &closed_sockfd,
      .dtor = NULL};
}

static umodem_event_t quectel_m65_handle_qirdi(const char* buf, size_t len) {
  // Expect: "+QIRDI: 0,1,<sockfd>"
  const char* colon = memchr(buf, ':', len);
  if (!colon) return (umodem_event_t){0};

  size_t remaining = buf + len - (colon + 1);
  const char* comma = memchr(colon + 1, ',', remaining);
  if (!comma || comma >= buf + len) return (umodem_event_t){0};

  remaining = buf + len - (comma + 1);
  comma = memchr(comma + 1, ',', remaining);
  if (!comma) return (umodem_event_t){0};

  int sockfd;
  if (!UMODEM_STRTOI(comma + 1, 0, QUECTEL_M65_MAX_SOCKETS - 1, &sockfd))
    return (umodem_event_t){0};

  if (sockfd >= 0 && sockfd < QUECTEL_M65_MAX_SOCKETS)
    return (umodem_event_t){.event_flag = UMODEM_EVENT_SOCK_DATA_RECEIVED,
        .data = &g_sockets[sockfd].sockfd,
        .dtor = NULL};
  return (umodem_event_t){0};
}

static umodem_event_t quectel_m65_handle_qmtopen(const char* buf, size_t len) {
  // Expect: "+QMTOPEN: <sockfd>,<result>"
  char* qmtopen = memchr(buf, ':', len);
  if (!qmtopen) return (umodem_event_t){0};

  int sockfd;
  if (!UMODEM_STRTOI(qmtopen + 2, 0, QUECTEL_M65_MAX_MQTT_CONNS - 1, &sockfd)) {
    g_mqtt_conns[sockfd].context_open = -1;
    return (umodem_event_t){0};
  }

  char* comma = memchr(buf, ',', len);
  int result;
  if (!UMODEM_STRTOI(comma + 1, 0, INT_MAX, &result)) {
    g_mqtt_conns[sockfd].context_open = -1;
    return (umodem_event_t){0};
  }

  g_mqtt_conns[sockfd].context_open = (result == 0) ? 1 : -1;
  return (umodem_event_t){0};
}

static umodem_event_t quectel_m65_handle_qmtconn(const char* buf, size_t len) {
  // Expect: "+QMTCONN: <sockfd>,<result>,<retcode>"
  char* qmtconn = memchr(buf, ':', len);
  if (!qmtconn) return (umodem_event_t){0};

  int sockfd;
  if (!UMODEM_STRTOI(qmtconn + 2, 0, QUECTEL_M65_MAX_MQTT_CONNS - 1, &sockfd)) {
    g_mqtt_conns[sockfd].context_open = -1;
    g_mqtt_conns[sockfd].sock.connected = -1;
    return (umodem_event_t){0};
  }

  char* comma = memchr(buf, ',', len); // result
  int result;
  if (!UMODEM_STRTOI(comma + 1, 0, INT_MAX, &result)) {
    g_mqtt_conns[sockfd].context_open = -1;
    g_mqtt_conns[sockfd].sock.connected = -1;
    return (umodem_event_t){0};
  }

  if (result != 0) {
    g_mqtt_conns[sockfd].context_open = -1;
    g_mqtt_conns[sockfd].sock.connected = -1;
    return (umodem_event_t){0};
  }

  size_t remaining = buf + len - (comma + 1);
  comma = memchr(comma + 1, ',', remaining); // retcode
  int retcode;
  if (!UMODEM_STRTOI(comma + 1, 0, INT_MAX, &retcode)) {
    g_mqtt_conns[sockfd].context_open = -1;
    g_mqtt_conns[sockfd].sock.connected = -1;
    return (umodem_event_t){0};
  }

  if (retcode == 0) {
    g_mqtt_conns[sockfd].sock.connected = 1;
    return (umodem_event_t){.event_flag = UMODEM_EVENT_SOCK_CONNECTED,
        .data = &g_mqtt_conns[sockfd].sock.sockfd,
        .dtor = NULL};
  }

  g_mqtt_conns[sockfd].context_open = -1;
  g_mqtt_conns[sockfd].sock.connected = -1;
  return (umodem_event_t){0};
}

static umodem_event_t quectel_m65_handle_qmtpub(const char* buf, size_t len) {
  // Expect: "+QMTPUB: <sockfd>,<msg_id>,<result>"
  char* qmtpub = memchr(buf, ':', len);
  if (!qmtpub) return (umodem_event_t){0};

  int sockfd;
  if (!UMODEM_STRTOI(qmtpub + 2, 0, QUECTEL_M65_MAX_MQTT_CONNS - 1, &sockfd))
    return (umodem_event_t){0};

  char* comma = memchr(buf, ',', len); // msg_id
  int msg_id;
  if (!UMODEM_STRTOI(comma + 1, 0, INT_MAX, &msg_id))
    return (umodem_event_t){0};

  size_t remaining = buf + len - (comma + 1);
  comma = memchr(comma + 1, ',', remaining); // result
  int result;
  if (!UMODEM_STRTOI(comma + 1, 0, INT_MAX, &result))
    return (umodem_event_t){0};

  if (result == 0)
    return (umodem_event_t){.event_flag = UMODEM_EVENT_MQTT_DATA_PUBLISHED,
        .data = mqtt_pop_message(msg_id),
        .dtor = umodem_event_mqtt_pub_dtor};

  return (umodem_event_t){0};
}

static umodem_event_t quectel_m65_handle_qmtrecv(const char* buf, size_t len) {
  // Expect: "+QMTRECV: <sockfd>,<msg_id>,<topic>,<payload>"
  char* qmtrecv = memchr(buf, ':', len);
  if (!qmtrecv) return (umodem_event_t){0};

  int sockfd;
  if (!UMODEM_STRTOI(qmtrecv + 2, 0, QUECTEL_M65_MAX_MQTT_CONNS - 1, &sockfd))
    return (umodem_event_t){0};

  char* comma1 = memchr(buf, ',', len); // msg_id
  if (!comma1) return (umodem_event_t){0};

  size_t remaining = buf + len - (comma1 + 1);
  comma1 = memchr(comma1 + 1, ',', remaining); // topic
  if (!comma1) return (umodem_event_t){0};

  remaining = buf + len - (comma1 + 1);
  char* comma2 = memchr(comma1 + 1, ',', remaining); // payload
  if (!comma2) return (umodem_event_t){0};

  char* payload_start = comma2 + 1;
  if (payload_start <= (buf + len)) {
    umodem_event_mqtt_data_t* event_data =
        mqtt_get_message_by_topic(comma1 + 1, comma2 - comma1 - 1);
    if (!event_data) return (umodem_event_t){0};

    size_t payload_size = (buf + len) - payload_start - 2; // 2 is \r\n
    if (payload_size <= 0) return (umodem_event_t){0};
    event_data->data = calloc(payload_size, 1);
    if (!event_data->data) return (umodem_event_t){0};

    memcpy(event_data->data, payload_start, payload_size);
    event_data->data_len = payload_size;
    return (umodem_event_t){.event_flag = UMODEM_EVENT_MQTT_DATA_RECEIVED,
        .data = event_data,
        .dtor = umodem_event_mqtt_sub_dtor};
  }

  return (umodem_event_t){0};
}

umodem_event_t quectel_m65_handle_urc(const char* buf, size_t len) {
  if (UMODEM_MEMMEM(buf, len, "+PDP DEACT", 11))
    return quectel_m65_handle_pdp_deact(buf, len);
  else if (UMODEM_MEMMEM(buf, len, "+CREG:", 6)) {
    // Skip if contains comma (not URC)
    if (UMODEM_MEMMEM(buf, len, ",", 1)) return (umodem_event_t){0};
    return quectel_m65_handle_creg(buf, len);
  } else if (UMODEM_MEMMEM(buf, len, "CONNECT OK", 10))
    return quectel_m65_handle_connect_ok(buf, len);
  else if (UMODEM_MEMMEM(buf, len, "CONNECT FAIL", 12))
    return quectel_m65_handle_connect_fail(buf, len);
  else if (UMODEM_MEMMEM(buf, len, "CLOSED", 6))
    return quectel_m65_handle_closed(buf, len);
  else if (UMODEM_MEMMEM(buf, len, "+QIRDI:", 7))
    return quectel_m65_handle_qirdi(buf, len);
  else if (UMODEM_MEMMEM(buf, len, "+QMTOPEN:", 9))
    return quectel_m65_handle_qmtopen(buf, len);
  else if (UMODEM_MEMMEM(buf, len, "+QMTCONN:", 9))
    return quectel_m65_handle_qmtconn(buf, len);
  else if (UMODEM_MEMMEM(buf, len, "+QMTPUB:", 8))
    return quectel_m65_handle_qmtpub(buf, len);
  else if (UMODEM_MEMMEM(buf, len, "+QMTRECV:", 9))
    return quectel_m65_handle_qmtrecv(buf, len);

  return (umodem_event_t){0};
}
