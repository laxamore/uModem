#include <string.h>

#include "umodem.h"
#include "umodem_hal.h"
#include "umodem_at.h"
#include "umodem_buffer.h"

void umodem_at_init()
{
  umodem_buffer_init();
  umodem_hal_init();
}

void umodem_at_deinit()
{
  umodem_hal_deinit();
}

int umodem_at_send(const char *cmd, char *response, size_t resp_len, uint32_t timeout_ms)
{
  if (umodem_hal_send((const uint8_t *)cmd, strlen(cmd)) < 0)
    return -1;

  uint32_t time_start = umodem_hal_millis();
  while (umodem_hal_millis() - time_start <= timeout_ms)
  {
    umodem_poll(); // Process URCs

    umodem_hal_lock();

    int pos = -1;
    size_t match_len = 0;
    int result = -1;

    // Success responses
    if ((pos = umodem_buffer_find((uint8_t *)"SEND OK\r\n", 9)) >= 0)
    {
      match_len = 9;
      result = 0;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"OK\r\n", 4)) >= 0)
    {
      match_len = 4;
      result = 0;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"> ", 2)) >= 0)
    {
      match_len = 2;
      result = 0;
    }

    // Error responses
    else if ((pos = umodem_buffer_find((uint8_t *)"ERROR\r\n", 7)) >= 0)
    {
      match_len = 7;
      result = -1;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"+CME ERROR:", 11)) >= 0)
    {
      int end_pos = umodem_buffer_find_from((uint8_t *)"\r\n", 2, pos);
      if (end_pos >= pos + 11)
      {
        match_len = (size_t)(end_pos - pos) + 2; // include \r\n
        result = -1;
      }
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"+CMS ERROR:", 11)) >= 0)
    {
      int end_pos = umodem_buffer_find_from((uint8_t *)"\r\n", 2, pos);
      if (end_pos >= pos + 11)
      {
        match_len = (size_t)(end_pos - pos) + 2;
        result = -1;
      }
    }

    if (match_len > 0)
    {
      size_t total_len = (size_t)pos;
      char buf[total_len + match_len + 1];

      umodem_buffer_pop(buf, total_len + match_len);
      buf[total_len] = '\0';

      if (response && resp_len > 0)
      {
        char *payload_start = buf;
        char *payload_end = buf + total_len; // points to start of "OK\r\n"

        // Skip leading whitespace and empty lines
        while (payload_start < payload_end && (*payload_start == '\r' || *payload_start == '\n'))
          payload_start++;

        // Trim trailing \r\n from payload_end
        uint8_t max_trim = 4; // max 2 trailing \r\n pairs
        while (payload_end > payload_start && max_trim > 0 &&
               (*(payload_end - 1) == '\r' || *(payload_end - 1) == '\n'))
        {
          payload_end--;
          max_trim--;
        }

        // Find last occurrence of "\r\n\r\n" before payload_end
        char *last_empty_line = NULL;
        char *search = payload_start;
        while (search < payload_end - 3)
        {
          if (memcmp(search, "\r\n\r\n", 4) == 0)
            last_empty_line = search;
          search++;
        }

        if (last_empty_line && last_empty_line + 4 <= payload_end)
          payload_start = last_empty_line + 4;

        size_t payload_len = (size_t)(payload_end - payload_start);
        if (payload_len >= resp_len)
          payload_len = resp_len - 1;

        if (payload_len > 0)
          memcpy(response, payload_start, payload_len);
        response[payload_len] = '\0';
      }

      umodem_hal_unlock();
      return result;
    }

    umodem_hal_unlock();
    umodem_hal_delay_ms(10);
  }

  return -1; // timeout
}