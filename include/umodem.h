#ifndef uMODEM_H_
#define uMODEM_H_

#include <stdint.h>
#include <stddef.h>

#include "umodem_config.h"
#include "umodem_event.h"

#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum
  {
    UMODEM_OK = 0,                // Success
    UMODEM_ERR = -1,              // General error
    UMODEM_TIMEOUT = -2,          // Operation timed out
    UMODEM_PARAM = -3,            // Invalid parameter
    UMODEM_SIM_NOT_INSERTED = -4, // Sim not inserted
  } umodem_result_t;

  typedef enum
  {
    UMODEM_SOCK,
    UMODEM_HTTP,
    UMODEM_MQTT,
    UMODEM_PPP
  } umodem_mode_t;

  typedef struct
  {
    char apn[32];
    char user[32];
    char pass[32];
  } umodem_apn_t;

  /**
   * @brief Event callback function signature.
   *
   * @param event_t    Event information structure.
   * @param user_ctx   User-defined context pointer (set via registration).
   * @param len        Length of data in bytes. May be 0.
   */
  typedef void (*umodem_event_cb_t)(umodem_event_t *event, void *user_ctx);

  /**
   * @brief Initialize the modem library.
   * Must be called once before any other function.
   *
   * @return UMODEM_OK on success.
   */
  umodem_result_t umodem_init(umodem_apn_t *apn);

  /**
   * @brief Deinitialize the modem library and power off modem.
   *
   * @return UMODEM_OK on success.
   */
  umodem_result_t umodem_deinit(void);

  /**
   * @brief Poll the modem state and process events.
   * Must be called periodically (e.g., every 10–100 ms).
   */
  void umodem_poll(void);

  /**
   * @brief Power on the modem hardware (if controllable).
   *
   * @return UMODEM_OK on success.
   */
  umodem_result_t umodem_power_on(void);

  /**
   * @brief Power off the modem hardware.
   *
   * @return UMODEM_OK on success.
   */
  umodem_result_t umodem_power_off(void);

  umodem_result_t umodem_get_imei(char *buf, size_t buf_size);

  umodem_result_t umodem_get_iccid(char *buf, size_t buf_size);

  umodem_result_t umodem_get_signal_quality(int *rssi, int *ber);

  /**
   * @brief Register an event callback.
   *
   * @param cb        Callback function (set to NULL to unregister).
   * @param user_ctx  User-defined pointer passed to callback.
   */
  void umodem_register_event_callback(umodem_event_cb_t cb, void *user_ctx);

#ifdef __cplusplus
}
#endif

#endif