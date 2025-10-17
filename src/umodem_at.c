#include <string.h>
#include <stdlib.h>

#include "umodem.h"
#include "umodem_hal.h"
#include "umodem_at.h"
#include "umodem_buffer.h"

void umodem_at_init()
{
  umodem_hal_init();
}

void umodem_at_deinit()
{
  umodem_hal_deinit();
}

umodem_result_t umodem_at_send(const char *cmd, char *response, size_t resp_len, uint32_t timeout_ms)
{
  if (umodem_hal_send((const uint8_t *)cmd, strlen(cmd)) < 0)
    return UMODEM_ERR;

  uint32_t time_start = umodem_hal_millis();
  while (umodem_hal_millis() - time_start <= timeout_ms)
  {
    umodem_poll(); // Process URCs

    umodem_hal_lock();

    int pos = -1;
    size_t match_len = 0;
    umodem_result_t result = UMODEM_ERR;

    // Success responses
    if ((pos = umodem_buffer_find((uint8_t *)"\r\nSEND OK\r\n", 11)) >= 0)
    {
      pos += 2;
      match_len = 9;
      result = UMODEM_OK;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"\r\nCLOSE OK\r\n", 12)) >= 0)
    {
      pos += 2;
      match_len = 10;
      result = UMODEM_OK;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"\r\nDEACT OK\r\n", 12)) >= 0)
    {
      pos += 2;
      match_len = 10;
      result = UMODEM_OK;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"\r\nOK\r\n", 6)) >= 0)
    {
      pos += 2;
      match_len = 4;
      result = UMODEM_OK;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"\r\n> ", 4)) >= 0)
    {
      pos += 2;
      match_len = 2;
      result = UMODEM_OK;
    }

    // Error responses
    else if ((pos = umodem_buffer_find((uint8_t *)"\r\nERROR\r\n", 9)) >= 0)
    {
      pos += 2;
      match_len = 7;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"+CME ERROR:", 11)) >= 0)
    {
      int end_pos = umodem_buffer_find_from((uint8_t *)"\r\n", 2, pos);
      if (end_pos >= pos + 11)
        match_len = (size_t)(end_pos - pos) + 2; // include \r\n
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"+CMS ERROR:", 11)) >= 0)
    {
      int end_pos = umodem_buffer_find_from((uint8_t *)"\r\n", 2, pos);
      if (end_pos >= pos + 11)
        match_len = (size_t)(end_pos - pos) + 2;
    }

    if (match_len > 0)
    {
      size_t total_len = (size_t)pos;
      uint8_t *buf = (uint8_t *)calloc(total_len + match_len + 1, 1);
      if (!buf)
      {
        umodem_hal_unlock();
        return UMODEM_ERR; // out of memory
      }

      umodem_buffer_pop(buf, total_len + match_len);
      buf[total_len] = '\0';

      if (response && resp_len > 0)
      {
        uint8_t *payload_start = buf;
        uint8_t *payload_end = buf + total_len; // points to start of "OK\r\n"

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
        uint8_t *last_empty_line = NULL;
        uint8_t *search = payload_start;
        while (search < payload_end - 3)
        {
          if (memcmp(search, (const uint8_t*)"\r\n\r\n", 4) == 0)
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
      free(buf);
      return result;
    }

    umodem_hal_unlock();
    umodem_hal_delay_ms(10);
  }

  return UMODEM_TIMEOUT; // timeout
}