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
    umodem_poll(); // Process URCs (removes known URC lines from buffer)

    umodem_hal_lock();

    int pos = -1;
    const char *match = NULL;
    size_t match_len = 0;
    int result = -1;

    // Success responses
    if ((pos = umodem_buffer_find((uint8_t *)"SEND OK\r\n", 9)) >= 0)
    {
      match = "SEND OK\r\n";
      match_len = 9;
      result = 0;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"OK\r\n", 4)) >= 0)
    {
      match = "OK\r\n";
      match_len = 4;
      result = 0;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"CONNECT\r\n", 9)) >= 0)
    {
      match = "CONNECT\r\n";
      match_len = 9;
      result = 0;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"> ", 2)) >= 0)
    {
      match = "> ";
      match_len = 2;
      result = 0;
    }

    // Error responses
    else if ((pos = umodem_buffer_find((uint8_t *)"ERROR\r\n", 7)) >= 0)
    {
      match = "ERROR\r\n";
      match_len = 7;
      result = -1;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"NO CARRIER\r\n", 12)) >= 0)
    {
      match = "NO CARRIER\r\n";
      match_len = 12;
      result = -1;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"NO DIALTONE\r\n", 13)) >= 0)
    {
      match = "NO DIALTONE\r\n";
      match_len = 13;
      result = -1;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"NO ANSWER\r\n", 11)) >= 0)
    {
      match = "NO ANSWER\r\n";
      match_len = 11;
      result = -1;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"BUSY\r\n", 6)) >= 0)
    {
      match = "BUSY\r\n";
      match_len = 6;
      result = -1;
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"+CME ERROR:", 11)) >= 0)
    {
      // +CME ERROR:<err> — find end of line
      int end_pos = umodem_buffer_find_from((uint8_t *)"\r\n", 2, pos);
      if (end_pos >= pos + 11)
      {
        match = "+CME ERROR:";
        match_len = (size_t)(end_pos - pos) + 2; // include \r\n
        result = -1;
      }
    }
    else if ((pos = umodem_buffer_find((uint8_t *)"+CMS ERROR:", 11)) >= 0)
    {
      // +CMS ERROR:<err>
      int end_pos = umodem_buffer_find_from((uint8_t *)"\r\n", 2, pos);
      if (end_pos >= pos + 11)
      {
        match = "+CMS ERROR:";
        match_len = (size_t)(end_pos - pos) + 2;
        result = -1;
      }
    }

    if (match != NULL)
    {
      size_t total_len = (size_t)pos;
      char buf[total_len + match_len + 1];

      umodem_buffer_pop(buf, total_len + match_len);
      buf[total_len] = '\0';

      char *start = strstr(buf, "\r\n");
      if (!start)
      {
        umodem_hal_unlock();
        return -1;
      }

      start += 2;

      if (total_len - (start - buf) < 0)
      {
        umodem_hal_unlock();
        return -1;
      }

      if (response != NULL && resp_len > 0)
      {
        char *end = strstr(start, "\r\n");
        size_t copy_len = (end - start < resp_len) ? end - start + 1 : resp_len;
        strncpy(response, start, copy_len);
        response[copy_len - 1] = '\0';
      }

      umodem_hal_unlock();
      return result;
    }

    umodem_hal_unlock();
    umodem_hal_delay_ms(10);
  }

  return -1; // timeout
}