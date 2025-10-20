#include "umodem.h"
#include "umodem_driver.h"

#if defined(UMODEM_QUECTEL_M65)
#include "drivers/quectel_m65.c.in"
#elif defined(UMODEM_SIMCOM_SIM800)
#include "drivers/simcom_sim800.c.in"
#else
#error "No Modem Drivers Selected"
#endif

umodem_result_t umodem_sock_init(void) {
  if (g_umodem_driver == NULL || g_umodem_driver->sock_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->sock_driver->sock_init();
}

umodem_result_t umodem_sock_deinit(void) {
  if (g_umodem_driver == NULL || g_umodem_driver->sock_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->sock_driver->sock_deinit();
}

int umodem_sock_create(umodem_sock_type_t type) {
  if (g_umodem_driver == NULL || g_umodem_driver->sock_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0 ||
      g_umodem_driver->sock_driver->sock_create == NULL)
    return -1;

  return g_umodem_driver->sock_driver->sock_create(type);
}

umodem_result_t umodem_sock_connect(int sockfd, const char* host,
    size_t host_len, uint16_t port, uint32_t timeout_ms) {
  if (g_umodem_driver == NULL || g_umodem_driver->sock_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0 ||
      g_umodem_driver->sock_driver->sock_connect == NULL)
    return UMODEM_ERR;

  return g_umodem_driver->sock_driver->sock_connect(
      sockfd, host, host_len, port, timeout_ms);
}

umodem_result_t umodem_sock_close(int sockfd) {
  if (g_umodem_driver == NULL || g_umodem_driver->sock_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0 ||
      g_umodem_driver->sock_driver->sock_close == NULL)
    return UMODEM_ERR;

  return g_umodem_driver->sock_driver->sock_close(sockfd);
}

int umodem_sock_send(int sockfd, const void* data, size_t len) {
  if (!g_umodem_driver || !g_umodem_driver->sock_driver ||
      !g_umodem_driver->umodem_initialized ||
      !g_umodem_driver->sock_driver->sock_send)
    return -1;
  return g_umodem_driver->sock_driver->sock_send(sockfd, data, len);
}

int umodem_sock_recv(int sockfd, void* buf, size_t len) {
  if (!g_umodem_driver || !g_umodem_driver->sock_driver ||
      !g_umodem_driver->umodem_initialized ||
      !g_umodem_driver->sock_driver->sock_recv)
    return -1;
  return g_umodem_driver->sock_driver->sock_recv(sockfd, buf, len);
}

umodem_result_t umodem_mqtt_init(void) {
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->mqtt_driver->mqtt_init();
}

umodem_result_t umodem_mqtt_deinit(void) {
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->mqtt_driver->mqtt_deinit();
}

int umodem_mqtt_connect(
    const char* host, uint16_t port, const umodem_mqtt_connect_opts_t* opts) {
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

#if defined(UMODEM_MQTT_CLIENT_ID_PREFIX) && (UMODEM_MQTT_CLIENT_ID_PREFIX != 0)
  if (!opts || !opts->client_id) return UMODEM_ERR;

  char final_client_id[strlen(opts->client_id) + 5 + 1];
  uint32_t rand_num = umodem_rand() % 10000; // 0–9999

  // Format: "<user_client_id>_<rand>"
  int len = snprintf(final_client_id, sizeof(final_client_id), "%s_%lu",
      opts->client_id, (unsigned long)rand_num);

  if (len <= 0 || (size_t)len >= sizeof(final_client_id))
    return UMODEM_ERR; // buffer overflow

  umodem_mqtt_connect_opts_t modified_opts = *opts;
  modified_opts.client_id = final_client_id;
  return g_umodem_driver->mqtt_driver->mqtt_connect(host, port, &modified_opts);
#else
  return g_umodem_driver->mqtt_driver->mqtt_connect(host, port, opts);
#endif
}

umodem_result_t umodem_mqtt_disconnect(int sockfd) {
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->mqtt_driver->mqtt_disconnect(sockfd);
}

umodem_result_t umodem_mqtt_subscribe(
    int sockfd, const char* topic, size_t topic_len, umodem_mqtt_qos_t qos) {
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return -1;

  return g_umodem_driver->mqtt_driver->mqtt_subscribe(
      sockfd, topic, topic_len, qos);
}

umodem_result_t umodem_mqtt_unsubscribe(
    int sockfd, const char* topic, size_t topic_len) {
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->mqtt_driver->mqtt_unsubscribe(
      sockfd, topic, topic_len);
}

umodem_result_t umodem_mqtt_publish(int sockfd, const char* topic,
    size_t topic_len, const void* payload, size_t len, umodem_mqtt_qos_t qos,
    int retain) {
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->mqtt_driver->mqtt_publish(
      sockfd, topic, topic_len, payload, len, qos, retain);
}
