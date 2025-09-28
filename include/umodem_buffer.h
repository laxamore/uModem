#ifndef uMODEM_BUFFER_H_
#define uMODEM_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * Initialize the internal ring buffer.
   * Must be called once before use.
   */
  void umodem_buffer_init(void);

  /**
   * Push data INTO the buffer (called by HAL on RX).
   * @return Number of bytes actually stored (may be < len if buffer full).
   */
  size_t umodem_buffer_push(const uint8_t *data, size_t len);

  /**
   * Pop data FROM the buffer (called by parser/core).
   * @return Number of bytes actually read.
   */
  size_t umodem_buffer_pop(uint8_t *dst, size_t len);

  /**
   * Find matching bytes in the buffer
   * @return -1 if not found or position in the buffer.
   */
  int umodem_buffer_find(uint8_t *find);

  /**
   * Discard all data in the buffer.
   */
  void umodem_buffer_flush(void);

#ifdef __cplusplus
}
#endif

#endif