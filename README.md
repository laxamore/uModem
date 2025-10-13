# uModem

**uModem** is a lightweight, modular, and portable C library for managing AT-command-based cellular modems.  

## 🚀 Getting Started

### Configure
Edit `include/umodem_config.h` to select your modem family:
```c
#define UMODEM_MODEM_QUECTEL_M65
```

## 🧩 Example Usage

```c
#include "umodem.h"
#include "umodem_sock.h"

void on_event(umodem_event_info_t *info, void *ctx) {
    if (info->event == UMODEM_EVENT_SOCK_CONNECTED)
        printf("Socket connected!\n");
}

int main(void) {
    umodem_apn_t apn = { .apn = "internet", .user = "", .pass = "" };
    umodem_power_on();
    umodem_init(&apn);

    umodem_register_event_callback(on_event, NULL);

    char imei[32];
    umodem_get_imei(imei, sizeof(imei));
    printf("IMEI: %s\n", imei);

    umodem_sock_init();
    int sock = umodem_sock_create(UMODEM_SOCK_TYPE_TCP);
    umodem_sock_connect(sock, "example.com", 80);

    umodem_deinit();
    umodem_power_off();
}
```

## 🧠 Porting Guide (HAL)

To support a new platform, implement the following functions in your HAL:

```c
void umodem_hal_init(void);
void umodem_hal_deinit(void);
int  umodem_hal_send(const uint8_t *buf, size_t len);
int  umodem_hal_read(uint8_t *buf, size_t len);
uint32_t umodem_hal_millis(void);
void umodem_hal_delay_ms(uint32_t ms);
void umodem_hal_lock(void); // Optional: for thread safety
void umodem_hal_unlock(void); // Optional: for thread safety
```

For example, see [`examples/socket/posix/posix_hal.cpp`](examples/socket/posix/posix_hal.cpp).

---

## 🧩 Planned Extensions

- [ ] HTTP/HTTPS client driver
- [ ] MQTT driver
- [ ] PPP dial-up mode
- [ ] CoAP
