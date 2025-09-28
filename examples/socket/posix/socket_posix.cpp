#include <stdio.h>

#include "umodem.h"

int main()
{
  if (umodem_power_on() != UMODEM_OK) return -1;
  if (umodem_init(1000) != UMODEM_OK) {
    printf("Failed to initialize uModem\n");
    return -1;
  }

  umodem_deinit();  
  umodem_power_off();
  return 0;
}