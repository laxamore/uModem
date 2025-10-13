#ifndef uMODEM_AT_H_
#define uMODEM_AT_H_

#include "stdint.h"
#include "stddef.h"

#include "umodem.h"

#ifdef __cplusplus
extern "C"
{
#endif

  void umodem_at_init(void);

  void umodem_at_deinit(void);

  umodem_result_t umodem_at_send(const char *cmd, char *response, size_t resp_len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif