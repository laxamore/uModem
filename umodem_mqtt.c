#include <string.h>
#include <stdio.h>

#include "umodem_mqtt.h"
#include "umodem_driver.h"
#include "umodem_core.h"

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

  return g_umodem_driver->mqtt_driver->mqtt_subscribe(sockfd, topic, topic_len, qos);
}

umodem_result_t umodem_mqtt_unsubscribe(
    int sockfd, const char* topic, size_t topic_len) {
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->mqtt_driver->mqtt_unsubscribe(sockfd, topic, topic_len);
}

umodem_result_t umodem_mqtt_publish(int sockfd, const char* topic, size_t topic_len,
    const void* payload, size_t len, umodem_mqtt_qos_t qos, int retain) {
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->mqtt_driver->mqtt_publish(
      sockfd, topic, topic_len, payload, len, qos, retain);
}
