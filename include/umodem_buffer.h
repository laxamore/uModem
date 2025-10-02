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
   *
   * @return Number of bytes actually stored.
   */
  size_t umodem_buffer_push(const uint8_t *data, size_t len);

  /**
   * Pop data FROM the buffer.
   * @return Number of bytes actually read or -1 if failed.
   */
  int umodem_buffer_pop(uint8_t *dst, size_t len);

  /**
   * Peek data FROM the buffer.
   *
   * @return Number of bytes actually read or -1 if failed.
   */
  int umodem_buffer_peek(uint8_t *dst, size_t len);

  /**
   * Find matching bytes in the buffer
   *
   * @return -1 if not found or position in the buffer.
   */
  int umodem_buffer_find(uint8_t *expect, size_t len);

  /**
   * Discard all data in the buffer.
   */
  void umodem_buffer_flush(void);

  /**
   * Find matching bytes in the buffer starting from a given offset.
   *
   * @param pattern Pattern to search for
   * @param pattern_len Length of pattern
   * @param start_offset Logical offset in buffer to start searching from (0 = oldest byte)
   *
   * @return -1 if not found, or logical position in buffer (same as find())
   */
  int umodem_buffer_find_from(const uint8_t *pattern, size_t pattern_len, size_t start_offset);

  /**
   * Peek 'len' bytes from the buffer starting at logical 'offset'.
   * offset=0 means the oldest byte (ring.tail).
   * 
   * @return number of bytes read, or -1 on error.
   */
  int umodem_buffer_peek_from(uint8_t *dst, size_t offset, size_t len);

#ifdef __cplusplus
}
#endif

#endif