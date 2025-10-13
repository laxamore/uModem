/**
 * @file umodem_config.h
 * @brief Compile-time configuration for uModem.
 *
 * Override any setting by defining it before including this file,
 * or edit defaults below for your project.
 */

#ifndef uMODEM_CONFIG_H_
#define uMODEM_CONFIG_H_

/* Select your modem vendor/family.
 * Uncomment ONE of the following:
 */
#define UMODEM_MODEM_QUECTEL_M65

/* Size of the RX ring buffer used by parser (bytes). */
#ifndef UMODEM_RX_BUF_SIZE
#define UMODEM_RX_BUF_SIZE 256
#endif

/* Default command timeout in milliseconds for synchronous commands */
#ifndef UMODEM_CMD_TIMEOUT_MS
#define UMODEM_CMD_TIMEOUT_MS 5000
#endif

#define UMODEM_MQTT_CLIENT_ID_PREFIX 1

#endif