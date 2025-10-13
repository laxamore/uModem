#include <cstring>
#include <signal.h>
#include <stdio.h>

#include "umodem.h"
#include "umodem_mqtt.h"

static void on_umodem_event(umodem_event_info_t *event_info, void *user_ctx) {}

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

  int sockfd;
  umodem_mqtt_will_t will_opts = {
      .topic = "test_topic",
      .message = "any_message",
      .qos = UMODEM_MQTT_QOS_2,
      .retain = 1,
  };
  umodem_mqtt_connect_opts_t opts = {
      .client_id = "testumodem",
      .keepalive = 120,
      .will = &will_opts,
      .delivery_timeout_in_seconds = 5,
  };

  if (umodem_mqtt_init() != UMODEM_OK)
  {
    printf("Failed to initialize MQTT\n");
    goto cleanup;
  }

  printf("MQTT Initialized\n");

  sockfd = umodem_mqtt_connect("broker.hivemq.com", 1883, &opts);
  if (sockfd <= 0)
  {
    printf("Failed to connect MQTT\n");
    goto cleanup;
  }

  printf("Connected to MQTT Broker\n");

  for (int i = 0; i < 5; i++)
  {
    char payload[] = "hello_world";
    if (umodem_mqtt_publish(sockfd, "test/umodem", payload, sizeof(payload),
                            UMODEM_MQTT_QOS_2, 0))
      printf("Failed to publish MQTT message.\n");
    sleep(5);
  }

  if (umodem_mqtt_disconnect(sockfd) != UMODEM_OK)
    printf("Failed to disconnect MQTT\n");
  else
    printf("Disconnected from MQTT Broker\n");

cleanup:
  umodem_mqtt_deinit();
  umodem_deinit();
  umodem_power_off();
  return 0;
}
