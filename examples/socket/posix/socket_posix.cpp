#include <signal.h>
#include <stdio.h>

#include "umodem.h"
#include "umodem_sock.h"

static void sig_handler(int signo)
{
  printf("\nSignal received, shutting down...\n");

  // Cleanup
  umodem_deinit();
  umodem_power_off();

  _exit(0); // exit immediately
}

static void setup_signal_handlers(void)
{
  struct sigaction sa;
  sa.sa_handler = sig_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGINT, &sa, NULL);  // Ctrl+C
  sigaction(SIGTERM, &sa, NULL); // kill
}

umodem_apn_t apn_config = {
    .apn = "internet",
    .user = "",
    .pass = "",
};

int main()
{
  setup_signal_handlers();

  if (umodem_power_on() != UMODEM_OK)
    return -1;

  if (umodem_init(&apn_config) != UMODEM_OK)
  {
    printf("Failed to initialize uModem\n");
    umodem_power_off();
    return -1;
  }

  printf("uModem Initialized\n");

  char imei[32];
  if (umodem_get_imei(imei, sizeof(imei)) == UMODEM_OK)
    printf("IMEI: %s\n", imei);

  sleep(2); // wait a bit for signal quality to stabilize

  int rssi = 0, ber = 0;
  if (umodem_get_signal_quality(&rssi, &ber) == UMODEM_OK)
    printf("Signal Quality: RSSI=%d, BER=%d\n", rssi, ber);

  char iccid[32];
  if (umodem_get_iccid(iccid, sizeof(iccid)) == UMODEM_OK)
    printf("ICCID: %s\n", iccid);

  if (umodem_sock_init() != UMODEM_OK)
    printf("Failed to initialize socket interface\n");

  int sockfd = umodem_sock_create(UMODEM_SOCK_TCP);
  if (sockfd < 0)
    printf("Failed to create socket\n");
  else
  {
    printf("Socket created with fd: %d\n", sockfd);
    const char host[] = "tcpbin.com";
    uint16_t port = 4242;
    if (umodem_sock_connect(sockfd, host, sizeof(host) - 1, port, 10000) == UMODEM_OK)
      printf("Socket connected to %s:%d\n", host, port);
    else
      printf("Failed to connect socket\n");

    // Close socket
    if (umodem_sock_close(sockfd) == UMODEM_OK)
      printf("Socket closed\n");
    else
      printf("Failed to close socket\n");
  }

  umodem_sock_deinit();
  umodem_deinit();
  umodem_power_off();
  return 0;
}