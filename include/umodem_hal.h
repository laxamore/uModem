/**
 * @file umodem_hal.h
 * @brief Hardware Abstraction Layer (HAL) interface for uModem.
 *
 * This header defines the minimal set of functions that **must be implemented**
 * by the user or port layer (e.g., in `ports/posix/`, `ports/stm32/`, etc.)
 * to adapt uModem to a specific hardware platform or transport (UART, USB, TCP, etc.).
 *
 * ### Key Responsibilities of the HAL Implementation:
 *
 * 1. **Transmit**: Implement `umodem_hal_send()` to send AT commands to the modem.
 * 2. **Receive**: **Crucially**, whenever data is received from the modem
 *    (e.g., in a UART RX interrupt, USB callback, or socket read loop),
 *    the HAL **MUST** call `umodem_buffer_push()` (from `umodem_buffer.h`)
 *    to deliver the bytes to the uModem core.
 *
 *    Example (in your HAL code):
 *    ```c
 *    // Inside UART RX handler or polling loop:
 *    uint8_t byte = read_uart_byte();
 *    umodem_buffer_push(&byte, 1);
 *    ```
 *
 * 3. **Timing**: Provide monotonic millisecond time via `umodem_hal_millis()`.
 * 4. **Synchronization (if needed)**: If receive callbacks run in an ISR
 *    or a different thread than `umodem_poll()`, implement `umodem_hal_lock()`
 *    and `umodem_hal_unlock()` to protect shared state (e.g., the ring buffer).
 *    If not needed, provide empty stubs.
 *
 * ### Thread/ISR Safety Note:
 * The uModem core is **not thread-safe**. If your HAL receives data in an ISR
 * or RTOS task, you **must** use `umodem_hal_lock/unlock` around any access
 * to uModem internal state (especially when calling `umodem_buffer_push`).
 *
 * All HAL functions may be called from interrupt context unless otherwise noted.
 */

#ifndef uMODEM_HAL_H_
#define uMODEM_HAL_H_

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
   */
  int umodem_hal_read();

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

  /**
   * @brief Set hardware power on if any for the module.
   *
   * Must be implemented even if empty.
   */
  void umodem_hal_set_power_on(void);

  /**
   * @brief Set hardware power off if any for the module.
   *
   * Must be implemented even if empty.
   */
  void umodem_hal_set_power_off(void);

#ifdef __cplusplus
}
#endif

#endif