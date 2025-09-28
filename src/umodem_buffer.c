#include <string.h>
#include <stdint.h>

#include "umodem_config.h"
#include "umodem_buffer.h"

typedef struct
{
  uint8_t buf[UMODEM_RX_BUF_SIZE];
  size_t head;
  size_t tail;
  size_t count;
} ring_buffer_t;

static ring_buffer_t ring;

void umodem_buffer_init(void)
{
  ring.head = 0;
  ring.tail = 0;
  ring.count = 0;
  memset(ring.buf, 0, sizeof(ring.buf));
}

size_t umodem_buffer_push(const uint8_t *data, size_t len)
{
  size_t free_space;
  size_t used;

  if (ring.head >= ring.tail)
    used = ring.head - ring.tail;
  else
    used = UMODEM_RX_BUF_SIZE - (ring.tail - ring.head);

  free_space = UMODEM_RX_BUF_SIZE - 1 - used; // -1 to distinguish full vs empty

  // If incoming data is larger than free space → drop oldest
  if (len > free_space)
  {
    size_t drop = len - free_space;
    ring.tail = (ring.tail + drop) % UMODEM_RX_BUF_SIZE;
  }

  // How much space remains until buffer end
  size_t space_end = UMODEM_RX_BUF_SIZE - ring.head;

  if (len <= space_end)
  {
    // Single memcpy
    memcpy(&ring.buf[ring.head], data, len);
    ring.head = (ring.head + len) % UMODEM_RX_BUF_SIZE;
  }
  else
  {
    // Wrap around: two memcpy
    memcpy(&ring.buf[ring.head], data, space_end);
    memcpy(&ring.buf[0], data + space_end, len - space_end);
    ring.head = (len - space_end);
  }
}

size_t umodem_buffer_pop(uint8_t *dst, size_t len)
{
}

int umodem_buffer_find(uint8_t *expect)
{
  return -1;
}