#ifndef uMODEM_DRIVER_H_
#define uMODEM_DRIVER_H_

#include "umodem.h"

#include "umodem_mqtt.h"
#include "umodem_sock.h"
#include "umodem_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief uModem event structure. */
struct umodem_event {
  /** @brief Event flag/type */
  umodem_event_flag_t event_flag;
  /** @brief Destructor for event data cleanup */
  void (*dtor)(umodem_event_t* self);
  /** @brief Pointer to event-specific data */
  void* data;
};

/** @brief Socket driver interface for modem.
 *
 * Provides function pointers for socket operations.
 */
typedef struct {
  /** @brief Initialize socket functionality on the modem.
   *
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*sock_init)(void);

  /** @brief Deinitialize socket functionality on the modem.
   *
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*sock_deinit)(void);

  /** @brief Create a new socket on the modem.
   *
   * @param type Socket type (UMODEM_SOCK_TCP or UMODEM_SOCK_UDP)
   * @return Socket file descriptor on success, -1 on failure
   */
  int (*sock_create)(umodem_sock_type_t type);

  /** @brief Connect a socket to a remote host on the modem.
   *
   * @param sockfd Socket file descriptor
   * @param host Remote host (IP address or hostname)
   * @param host_len Length of the host string
   * @param port Remote port
   * @param timeout_ms Connection timeout in milliseconds (0 for no wait)
   * 
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*sock_connect)(int sockfd, const char* host, size_t host_len,
      uint16_t port, uint32_t timeout_ms);

  /** @brief Close a socket on the modem.
   *
   * @param sockfd Socket file descriptor
   * 
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*sock_close)(int sockfd);

  /** @brief Send data over a socket on the modem.
   *
   * @param sockfd Socket file descriptor
   * @param data Pointer to data to send
   * @param len Length of data to send
   * 
   * @return Number of bytes sent on success, -1 on failure
   */
  int (*sock_send)(int sockfd, const uint8_t* data, size_t len);

  /** @brief Receive data from a socket on the modem.
   *
   * @param sockfd Socket file descriptor
   * @param buf Buffer to store received data
   * @param len Length of the buffer
   * 
   * @return Number of bytes received on success, -1 on failure
   */
  int (*sock_recv)(int sockfd, uint8_t* buf, size_t len);
} umodem_sock_driver_t;

typedef struct {
} umodem_http_driver_t;

typedef struct {
  /** @brief Initialize MQTT functionality on the modem.
   * 
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*mqtt_init)(void);

  /** @brief Deinitialize MQTT functionality on the modem.
   * 
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*mqtt_deinit)(void);

  /**
   * @brief Establish an MQTT connection.
   *
   * Performs configuration of MQTT session parameters (keepalive, timeout,
   * will message, etc.) and opens a connection to the broker.
   *
   * @param host  Broker hostname or IP
   * @param port  Broker port number
   * @param opts  MQTT connection options (client ID, credentials, etc.)
   *
   * @return
   *   MQTT socket index (>0) on success, -1 on error
   */
  int (*mqtt_connect)(
      const char* host, uint16_t port, const umodem_mqtt_connect_opts_t* opts);

  /** @brief Disconnect an MQTT connection.
   * 
   * @param sockfd MQTT socket index
   * 
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*mqtt_disconnect)(int sockfd);

  /** @brief Publish an MQTT message.
   * 
   * @param sockfd MQTT socket index
   * @param topic Topic string
   * @param topic_len Length of the topic string
   * @param payload Pointer to message payload
   * @param len Length of the message payload
   * @param qos Quality of Service level
   * @param retain Retain flag
   * 
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*mqtt_publish)(int sockfd, const char* topic,
      size_t topic_len, const void* payload, size_t len, umodem_mqtt_qos_t qos,
      int retain);

  /** @brief Subscribe to an MQTT topic.
   * 
   * @param sockfd MQTT socket index
   * @param topic Topic string
   * @param topic_len Length of the topic string
   * @param qos Quality of Service level
   * 
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*mqtt_subscribe)(
      int sockfd, const char* topic, size_t topic_len, umodem_mqtt_qos_t qos);

  /** @brief Unsubscribe from an MQTT topic.
   * 
   * @param sockfd MQTT socket index
   * @param topic Topic string
   * @param topic_len Length of the topic string
   * 
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*mqtt_unsubscribe)(
      int sockfd, const char* topic, size_t topic_len);
} umodem_mqtt_driver_t;

typedef struct {
} umodem_ppp_driver_t;

/** @brief uModem driver structure.
 *
 * Contains function pointers for various modem operations.
 */
typedef struct {
  /** @brief APN configuration for the modem */
  umodem_apn_t* apn;

  /** @brief Initialize the modem.
   *
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*init)(void);

  /** @brief Deinitialize the modem.
   *
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*deinit)(void);

  /** @brief Get the IMEI of the modem.
   *
   * @param buf Buffer to store the IMEI string
   * @param buf_size Size of the buffer
   * 
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*get_imei)(char* buf, size_t buf_size);

  /** @brief Get the ICCID of the SIM card.
   *
   * @param buf Buffer to store the ICCID string
   * @param buf_size Size of the buffer
   * 
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*get_iccid)(char* buf, size_t buf_size);

  /** @brief Get signal quality (RSSI and BER).
   *
   * @param rssi Pointer to store RSSI value
   * @param ber Pointer to store BER value
   * 
   * @return UMODEM_OK on success, error code otherwise
   */
  umodem_result_t (*get_signal)(int* rssi, int* ber);

  /** @brief URC handler function pointer.
   *
   * @param buf Pointer to URC string buffer
   * @param len Length of the URC string
   *
   * @return umodem_event_t structure with event information
   */
  umodem_event_t (*handle_urc)(const char* buf, size_t len);

  /** @brief Socket driver interface */
  const umodem_sock_driver_t* sock_driver;

  /** @brief HTTP driver interface */
  const umodem_http_driver_t* http_driver;
  
  /** @brief MQTT driver interface */
  const umodem_mqtt_driver_t* mqtt_driver;

  /** @brief PPP driver interface */
  const umodem_ppp_driver_t* ppp_driver;

  /** @brief Modem initialization status */
  uint8_t umodem_initialized;
} umodem_driver_t;

extern umodem_driver_t* g_umodem_driver;

#ifdef __cplusplus
}
#endif

#endif