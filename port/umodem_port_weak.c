#include "umodem_port.h"

#define UMODEM_WEAK __attribute__((weak))

UMODEM_WEAK void umodem_hal_init(void) {}

UMODEM_WEAK void umodem_hal_deinit(void) {}

UMODEM_WEAK int umodem_hal_send(const uint8_t* buf, size_t len) {
  return -1;
}

UMODEM_WEAK int umodem_hal_read(uint8_t* buf, size_t len) {
  return 0;
}

UMODEM_WEAK uint32_t umodem_hal_millis(void) { return 0; }

UMODEM_WEAK void umodem_hal_delay_ms(uint32_t ms) {}

UMODEM_WEAK void umodem_hal_lock(void) {}

UMODEM_WEAK void umodem_hal_unlock(void) {}
