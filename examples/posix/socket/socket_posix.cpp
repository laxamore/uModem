#include <chrono>
#include <signal.h>
#include <stdio.h>
#include <cstring>

#include "umodem.h"
#include "umodem_sock.h"

using namespace std::chrono;

static int sockfd = -1;

static void on_umodem_event(umodem_event_info_t *event_info, void *user_ctx)
{
  switch (event_info->event)
  {
  case UMODEM_EVENT_SOCK_CONNECTED:
    printf("Socket connected! on fd %d\n", *(int *)event_info->event_data);
    break;
  case UMODEM_EVENT_SOCK_DATA_RECEIVED:
  {
    printf("Socket data received on fd %d\n", *(int *)event_info->event_data);

    // Read data
    char buf[128];
    int len = umodem_sock_recv(*(int *)event_info->event_data, buf, sizeof(buf) - 1);
    if (len > 0)
    {
      buf[len] = '\0';
      printf("Received data (%d bytes): %s\n", len, buf);
    }
    else
      printf("Receive error or no data\n");
    break;
  }
  case UMODEM_EVENT_SOCK_CLOSED:
    printf("Socket closed\n");
    break;
  default:
    break;
  }
}

static void sig_handler(int signo)
{
  printf("\nSignal received, shutting down...\n");

  // Cleanup
  if (sockfd > 0)
    umodem_sock_close(sockfd);

  umodem_sock_deinit();
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

static uint64_t current_millis()
{
  return duration_cast<milliseconds>(
             system_clock::now().time_since_epoch())
      .count();
}

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

  umodem_register_event_callback(on_umodem_event, NULL);

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
  {
    printf("Failed to initialize socket\n");
    umodem_sock_deinit();
    umodem_deinit();
    umodem_power_off();
  }

  sockfd = umodem_sock_create(UMODEM_SOCK_TCP);
  if (sockfd < 0)
  {
    printf("Failed to create socket\n");
    umodem_sock_deinit();
    umodem_deinit();
    umodem_power_off();
  }

  printf("Socket created with fd: %d\n", sockfd);

  const char host[] = "tcpbin.com";
  uint16_t port = 4242;

  if (umodem_sock_connect(sockfd, host, sizeof(host) - 1, port, 10000) == UMODEM_OK)
  {
    // Send data
    const char *msg = "Hello from uModem!\n";
    if (umodem_sock_send(sockfd, (uint8_t *)msg, strlen(msg)) > 0)
    {
      printf("Data sent\n");

      // Wait for response (event-driven)
      uint64_t start = current_millis();
      while (current_millis() - start <= 5000)
      {
        umodem_poll();
        usleep(1000);
      }
    }
    else
      printf("Send failed\n");
  }
  else
    printf("Connect command failed\n");

  umodem_sock_close(sockfd);
  printf("Socket closed\n");

  umodem_sock_deinit();
  umodem_deinit();
  umodem_power_off();
  return 0;
}