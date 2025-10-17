#ifndef uMODEM_DRIVER_QUECTEL_M65_URC_H_
#define uMODEM_DRIVER_QUECTEL_M65_URC_H_

#include "umodem_event.h"

#ifdef __cplusplus
extern "C" {
#endif

umodem_event_t quectel_m65_handle_urc(const char* buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
