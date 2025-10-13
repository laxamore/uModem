#include "umodem_mqtt.h"
#include "umodem_driver.h"

umodem_result_t umodem_mqtt_init(void)
{
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->mqtt_driver->mqtt_init();
}

umodem_result_t umodem_mqtt_deinit(void)
{
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->mqtt_driver->mqtt_deinit();
}

int umodem_mqtt_connect(const char *host, uint16_t port,
                        const umodem_mqtt_connect_opts_t *opts)
{
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->mqtt_driver->mqtt_connect(host, port, opts);
}

umodem_result_t umodem_mqtt_disconnect(int sockfd)
{
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->mqtt_driver->mqtt_disconnect(sockfd);
}

umodem_result_t umodem_mqtt_subscribe(int sockfd, const char *topic,
                                      umodem_mqtt_qos_t qos)
{
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->mqtt_driver->mqtt_subscribe(sockfd, topic, qos);
}

umodem_result_t umodem_mqtt_unsubscribe(int sockfd, const char *topic)
{
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->mqtt_driver->mqtt_unsubscribe(sockfd, topic);
}

umodem_result_t umodem_mqtt_publish(int sockfd, const char *topic,
                                    const void *payload, size_t len,
                                    umodem_mqtt_qos_t qos, int retain)
{
  if (g_umodem_driver == NULL || g_umodem_driver->mqtt_driver == NULL ||
      g_umodem_driver->umodem_initialized == 0)
    return UMODEM_ERR;

  return g_umodem_driver->mqtt_driver->mqtt_publish(sockfd, topic, payload, len,
                                                    qos, retain);
}
