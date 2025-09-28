/**
 * @file umodem.h
 * @brief High-level modem control library API.
 *
 * This library abstracts modem communication (e.g., AT command handling)
 * and provides a clean, event-driven interface for common operations.
 *
 * Thread safety: All functions are NOT thread-safe.
 * umodem_poll() must be called from a single thread.
 * Event callbacks are invoked from the umodem_poll() context.
 */

#ifndef uMODEM_H_
#define uMODEM_H_

#include <stdint.h>
#include <stddef.h>

#include "umodem_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* ==================== Result Codes ==================== */

  typedef enum
  {
    UMODEM_OK = 0,             ///< Success
    UMODEM_ERR = -1,           ///< General error
    UMODEM_TIMEOUT = -2,       ///< Operation timed out
    UMODEM_PARAM = -3,         ///< Invalid parameter
    UMODEM_NO_NETWORK = -4,    ///< Network not available
  } umodem_result_t;

  /* ==================== Event Types ==================== */

  typedef enum
  {
    UMODEM_EVENT_URC,         ///< Unsolicited Result Code (e.g., +CMTI, +CREG)
    UMODEM_EVENT_NET_UP,      ///< Successfully attached to cellular network
    UMODEM_EVENT_NET_DOWN,    ///< Detached or lost network registration
    UMODEM_EVENT_DATA_UP,     ///< Data connection (PDP context) activated
    UMODEM_EVENT_DATA_DOWN,   ///< Data connection deactivated
    UMODEM_EVENT_SMS_RECEIVED ///< New SMS received (if SMS monitoring enabled)
  } umodem_event_t;

  /* ==================== Callbacks ==================== */

  /**
   * @brief Event callback function signature.
   *
   * @param user_ctx  User-defined context pointer (set via registration).
   * @param event     Type of event that occurred.
   * @param data      Event-specific data (e.g., SMS text, URC string). May be NULL.
   * @param len       Length of data in bytes. May be 0.
   */
  typedef void (*umodem_event_cb_t)(void *user_ctx, umodem_event_t event, const void *data, size_t len);

  /* ==================== Core Lifecycle ==================== */

  /**
   * @brief Initialize the modem library.
   * Must be called once before any other function.
   * @return UMODEM_OK on success.
   */
  umodem_result_t umodem_init(uint32_t timeout_ms);

  /**
   * @brief Deinitialize the modem library and power off modem.
   * @return UMODEM_OK on success.
   */
  umodem_result_t umodem_deinit(void);

  /**
   * @brief Poll the modem state and process events.
   * Must be called periodically (e.g., every 10–100 ms).
   */
  void umodem_poll(void);

  /* ==================== Power & Readiness ==================== */

  /**
   * @brief Power on the modem hardware (if controllable).
   * @return UMODEM_OK on success.
   */
  umodem_result_t umodem_power_on(void);

  /**
   * @brief Power off the modem hardware.
   * @return UMODEM_OK on success.
   */
  umodem_result_t umodem_power_off(void);

  /**
   * @brief Check if modem is ready to accept commands.
   * @return 1 if ready, 0 if not, negative on error.
   */
  int umodem_is_ready(void);

  /* ==================== Network & Device Info ==================== */

  umodem_result_t umodem_get_imei(char *buf, size_t buf_size);
  umodem_result_t umodem_get_iccid(char *buf, size_t buf_size);
  umodem_result_t umodem_get_model(char *buf, size_t buf_size);
  umodem_result_t umodem_get_firmware_version(char *buf, size_t buf_size);
  umodem_result_t umodem_get_signal_quality(int *rssi, int *ber);

  umodem_result_t umodem_attach_network(uint32_t timeout_ms);
  umodem_result_t umodem_detach_network(void);

  /* ==================== Data Connection (PDP Context) ==================== */

  /**
   * @brief Activate data connection using the given APN.
   * Required before using HTTP, MQTT, etc.
   * @param apn Access Point Name (e.g., "internet", "iot.com")
   * @return UMODEM_OK on success.
   */
  umodem_result_t umodem_data_connect(const char *apn);

  /**
   * @brief Deactivate data connection.
   * @return UMODEM_OK on success.
   */
  umodem_result_t umodem_data_disconnect(void);

  /**
   * @brief Check if data connection is active.
   * @return 1 if connected, 0 if not, negative on error.
   */
  int umodem_is_data_connected(void);

  /* ==================== Event Registration ==================== */

  /**
   * @brief Register an event callback.
   * @param cb        Callback function (set to NULL to unregister).
   * @param user_ctx  User-defined pointer passed to callback.
   */
  void umodem_register_event_callback(umodem_event_cb_t cb, void *user_ctx);

#ifdef __cplusplus
}
#endif

#endif