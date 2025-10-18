#ifndef uMODEM_PORT_H_
#define uMODEM_PORT_H_

#include <stdint.h>
#include <stddef.h>

#include "umodem_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Initialize the modem transport (e.g., UART, USB, socket).
   *
   * This function should configure the physical interface (baud rate, pins, etc.)
   * and prepare it for communication.
   */
  void umodem_hal_init();

  /**
   * @brief Deinitialize the modem transport (e.g., UART, USB, socket).
   */
  void umodem_hal_deinit();

  /**
   * @brief Send data to the modem.
   *
   * This function must transmit the given bytes to the modem **synchronously**.
   *
   * @param buf Pointer to data to send.
   * @param len Number of bytes to send.
   * @return Number of bytes successfully sent (should equal `len`), or negative on error.
   */
  int umodem_hal_send(const uint8_t *buf, size_t len);

  /**
   * @brief Read any avialble data from modem.
   * 
   * This function read data from modem if available and passed to umodem buffer.
   * 
   * @param buf Pointer to readed data to store.
   * @param len Number of bytes can be stored.
   * @return Number of bytes successfully stored into the buffer, or negative on error.
   */
  int umodem_hal_read(uint8_t* buf, size_t len);

  /**
   * @brief Return monotonic time in milliseconds.
   *
   * Used for timeouts and delays. Must be **monotonic** (never reset or go backward).
   *
   * @return Milliseconds since arbitrary epoch (e.g., system boot).
   */
  uint32_t umodem_hal_millis(void);

  /**
   * @brief Block the current thread for a given number of milliseconds.
   *
   * May be implemented as a busy-wait or RTOS delay.
   *
   * @param ms Delay duration in milliseconds.
   */
  void umodem_hal_delay_ms(uint32_t ms);

  /**
   * @brief Acquire a lock to protect uModem internal state.
   *
   * Called by uModem core before accessing shared resources (e.g., ring buffer).
   * **Must be implemented** if HAL RX runs in ISR or another thread.
   * If not needed, provide an empty stub.
   *
   * This function may be called from interrupt context if RX uses interrupts.
   */
  void umodem_hal_lock(void);

  /**
   * @brief Release the lock acquired by `umodem_hal_lock()`.
   *
   * Must be implemented even if empty.
   */
  void umodem_hal_unlock(void);

#ifdef __cplusplus
}
#endif

#endif