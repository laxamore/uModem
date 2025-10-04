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
static size_t urc_scan_offset = 0;

void umodem_buffer_init(void)
{
  ring.head = 0;
  ring.tail = 0;
  ring.count = 0;
  memset(ring.buf, 0, sizeof(ring.buf));
}

size_t umodem_buffer_push(const uint8_t *data, size_t len)
{
  if (data == NULL)
    return 0;

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
    ring.count -= drop; // because we're about to add 'len', but net change is len - drop

    // Also adjust urc_scan_offset
    if (urc_scan_offset > drop)
      urc_scan_offset -= drop;
    else
      urc_scan_offset = 0;
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

  ring.count += len;
  return len;
}

int umodem_buffer_pop(uint8_t *dst, size_t len)
{
  if (ring.count < len)
    return -1;

  if (ring.tail + len > UMODEM_RX_BUF_SIZE)
  {
    size_t first_part = UMODEM_RX_BUF_SIZE - ring.tail;
    size_t second_part = len - first_part;
    if (dst)
    {
      memcpy(dst, &ring.buf[ring.tail], first_part);
      memcpy(dst + first_part, ring.buf, second_part);
    }
    ring.tail = second_part;
  }
  else
  {
    if (dst)
      memcpy(dst, &ring.buf[ring.tail], len);
    ring.tail = (ring.tail + len == UMODEM_RX_BUF_SIZE) ? 0 : ring.tail + len;
  }

  ring.count -= len;

  if (urc_scan_offset > len)
    urc_scan_offset -= len;
  else
    urc_scan_offset = 0;

  return len;
}

int umodem_buffer_peek_from(uint8_t *dst, size_t offset, size_t len)
{
  if (dst == NULL || len == 0)
    return -1;

  if (offset + len > ring.count)
    return -1;

  size_t read_pos = (ring.tail + offset) % UMODEM_RX_BUF_SIZE;

  if (read_pos + len <= UMODEM_RX_BUF_SIZE)
  {
    memcpy(dst, &ring.buf[read_pos], len);
  }
  else
  {
    size_t first_part = UMODEM_RX_BUF_SIZE - read_pos;
    size_t second_part = len - first_part;
    memcpy(dst, &ring.buf[read_pos], first_part);
    memcpy(dst + first_part, ring.buf, second_part);
  }

  return (int)len;
}

int umodem_buffer_peek(uint8_t *dst, size_t len)
{
  return umodem_buffer_peek_from(dst, 0, len);
}

int umodem_buffer_find(uint8_t *expect, size_t len)
{
  if (expect == NULL || len == 0 || ring.count < len)
    return -1;

  uint8_t linear_buf[ring.count];
  if (umodem_buffer_peek_from(linear_buf, 0, ring.count) != (int)ring.count)
    return -1;

  uint8_t *found = memmem(linear_buf, ring.count, expect, len);
  return found ? (int)(found - linear_buf) : -1;
}

int umodem_buffer_find_from(const uint8_t *pattern, size_t pattern_len, size_t start_offset)
{
  if (pattern == NULL || pattern_len == 0)
    return -1;

  if (start_offset >= ring.count)
    return -1;

  size_t search_len = ring.count - start_offset;
  if (search_len < pattern_len)
    return -1;

  uint8_t linear_buf[search_len];

  if (umodem_buffer_peek_from(linear_buf, start_offset, search_len) != (int)search_len)
    return -1;

  uint8_t *found = memmem(linear_buf, search_len, pattern, pattern_len);
  return found ? (int)(found - linear_buf) + (int)start_offset : -1;
}

int umodem_buffer_process_urcs(umodem_urc_handler_t handler)
{
  if (!handler)
    return -1;

  int lines_processed = 0;
  size_t offset = urc_scan_offset;

  while (offset < ring.count)
  {
    int pos = umodem_buffer_find_from((uint8_t *)"\r\n", 2, offset);
    if (pos < 0)
      break;

    size_t line_len = (size_t)(pos - offset) + 2;
    char buf[line_len + 1];

    if (umodem_buffer_peek_from((uint8_t *)buf, offset, line_len) != (int)line_len)
      break;

    buf[line_len] = '\0';

    // Call handler
    int event = handler((uint8_t *)buf, line_len);
    (void)event; // Let caller decide what to do with event

    lines_processed++;
    offset = pos + 2;
  }

  urc_scan_offset = offset;
  return lines_processed;
}