#include <chrono>
#include <cstring>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "umodem.h"
#include "umodem_mqtt.h"

// Define retry parameters
#define MQTT_CONNECT_MAX_RETRIES 5
#define MQTT_CONNECT_RETRY_DELAY_MS 2000 // 2 seconds

using namespace std::chrono;

static void on_umodem_event(umodem_event_t* event, void* user_ctx) {
  umodem_event_flag_t event_flag = umodem_event_get_flag(event);
  switch (event_flag) {
  case UMODEM_EVENT_SOCK_CONNECTED: {
    void* event_data = umodem_get_event_data(event);
    printf("Socket connected! on fd %d\n", *(int*)event_data);
    break;
  }
  case UMODEM_EVENT_MQTT_DATA_PUBLISHED: {
    umodem_event_mqtt_data_t* event_data =
        (umodem_event_mqtt_data_t*)umodem_get_event_data(event);
    printf("MQTT message published on fd %d\n", event_data->sockfd);
    break;
  }
  case UMODEM_EVENT_SOCK_CLOSED: printf("Socket closed\n"); break;
  default: break;
  }
}

static void sig_handler(int signo) {
  printf("\nSignal received, shutting down...\n");

  // Cleanup
  umodem_deinit();
  umodem_power_off();

  _exit(0); // exit immediately
}

static void setup_signal_handlers(void) {
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

static uint64_t current_millis() {
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
      .count();
}

int main() {
  setup_signal_handlers();

  if (umodem_power_on() != UMODEM_OK) return -1;

  if (umodem_init(&apn_config) != UMODEM_OK) {
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

  if (umodem_mqtt_init() != UMODEM_OK) {
    printf("Failed to initialize MQTT\n");
    umodem_mqtt_deinit();
    umodem_deinit();
    umodem_power_off();
    return -1;
  }

  printf("MQTT Initialized\n");

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
      .disable_clean_session = 0,
  };

  int sockfd = -1;
  int retry_count = 0;

  while (retry_count < MQTT_CONNECT_MAX_RETRIES) {
    sockfd = umodem_mqtt_connect("broker.hivemq.com", 1883, &opts);
    if (sockfd > 0) {
      printf("Connected to MQTT Broker\n");
      break;
    }

    retry_count++;
    printf("MQTT connection failed (attempt %d/%d). Retrying in %d ms...\n",
        retry_count, MQTT_CONNECT_MAX_RETRIES, MQTT_CONNECT_RETRY_DELAY_MS);

    usleep(MQTT_CONNECT_RETRY_DELAY_MS * 1000); // usleep takes microseconds
  }

  if (sockfd <= 0) {
    printf("Failed to connect to MQTT after %d attempts.\n",
        MQTT_CONNECT_MAX_RETRIES);
    umodem_mqtt_deinit();
    umodem_deinit();
    umodem_power_off();
    return -1;
  }

  uint64_t start = current_millis() - 5000;
  int count = 0;
  while (count < 5) {
    uint64_t now = current_millis();
    if (now - start >= 5000) {
      count++;
      start = now;
      char payload[] = "hello_world";
      char topic[] = "test/umodem";
      if (umodem_mqtt_publish(sockfd, topic, sizeof(topic), payload,
              sizeof(payload), UMODEM_MQTT_QOS_2, 0))
        printf("Failed to publish MQTT message.\n");
    }
    umodem_poll();
    usleep(1000);
  }

  sleep(5);
  umodem_poll();

  if (umodem_mqtt_disconnect(sockfd) != UMODEM_OK)
    printf("Failed to disconnect MQTT\n");
  else
    printf("Disconnected from MQTT Broker\n");

  umodem_mqtt_deinit();
  umodem_deinit();
  umodem_power_off();
  return 0;
}