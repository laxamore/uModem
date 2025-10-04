#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "umodem_hal.h"
#include "umodem_buffer.h"

static int serial_fd = -1;
static pthread_mutex_t hal_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t reader_thread;
static volatile int reader_running = 0;

// Default serial device path, can be changed
static const char *serial_dev = "/dev/ttyUSB0";

/* --- Reader thread: continuously read from serial and push to buffer --- */
static void *serial_reader(void *arg)
{
  (void)arg;
  uint8_t buf[256];

  while (reader_running)
  {
    ssize_t n = read(serial_fd, buf, sizeof(buf));
    if (n > 0)
    {
      // printf("%.*s", (int)n, buf); // debug print
      pthread_mutex_lock(&hal_mutex);
      umodem_buffer_push(buf, (size_t)n);
      pthread_mutex_unlock(&hal_mutex);
    }
    else if (n < 0)
    {
      if (errno != EAGAIN && errno != EINTR)
      {
        perror("serial read failed");
        usleep(1000);
      }
    }
    else
    {
      // no data, yield CPU
      usleep(1000);
    }
  }

  return nullptr;
}

static void umodem_hal_cleanup()
{
  if (serial_fd != -1)
  {
    reader_running = 0;                   // signal thread to exit
    pthread_join(reader_thread, nullptr); // wait for thread
    close(serial_fd);                     // close serial
    serial_fd = -1;
  }
}

void umodem_hal_deinit()
{
  umodem_hal_cleanup();
}

void umodem_hal_init()
{
  if (serial_fd != -1)
  {
    umodem_hal_cleanup();
  }

  serial_fd = open(serial_dev, O_RDWR | O_NOCTTY | O_SYNC);
  if (serial_fd < 0)
  {
    perror("Failed to open serial device");
    return;
  }

  int flags = fcntl(serial_fd, F_GETFL, 0);
  fcntl(serial_fd, F_SETFL, flags | O_NONBLOCK);

  struct termios tty;
  if (tcgetattr(serial_fd, &tty) != 0)
  {
    perror("tcgetattr failed");
    close(serial_fd);
    serial_fd = -1;
    return;
  }

  // Configure baudrate
  speed_t speed = B115200;
  cfsetospeed(&tty, speed);
  cfsetispeed(&tty, speed);

  // Raw mode
  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
  tty.c_iflag &= ~IGNBRK;                     // disable break processing
  tty.c_lflag = 0;                            // no signaling chars, no echo
  tty.c_oflag = 0;                            // no remapping, no delays
  tty.c_cc[VMIN] = 0;                         // read block if available, if not dont block
  tty.c_cc[VTIME] = 1;                        // 0.1s read timeout

  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // no SW flow control
  tty.c_cflag |= (CLOCAL | CREAD);        // enable receiver
  tty.c_cflag &= ~(PARENB | PARODD);      // no parity
  tty.c_cflag &= ~CSTOPB;                 // 1 stop bit
  tty.c_cflag &= ~CRTSCTS;                // no HW flow control

  if (tcsetattr(serial_fd, TCSANOW, &tty) != 0)
  {
    perror("tcsetattr failed");
    close(serial_fd);
    serial_fd = -1;
    return;
  }

  // Start reader thread
  reader_running = 1;
  if (pthread_create(&reader_thread, nullptr, serial_reader, nullptr) != 0)
  {
    perror("pthread_create failed");
    reader_running = 0;
  }
}

int umodem_hal_send(const uint8_t *buf, size_t len)
{
  if (serial_fd < 0)
    return -1;
  ssize_t written = write(serial_fd, buf, len);
  if (written < 0)
  {
    perror("write failed");
    return -1;
  }
  // printf("-> %.*s\n", (int)len, buf); // debug print
  return (int)written;
}

int umodem_hal_read(uint8_t *buf, size_t len)
{
  return 0;
}

uint32_t umodem_hal_millis(void)
{
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void umodem_hal_delay_ms(uint32_t ms)
{
  usleep(ms * 1000);
}

void umodem_hal_lock(void)
{
  pthread_mutex_lock(&hal_mutex);
}

void umodem_hal_unlock(void)
{
  pthread_mutex_unlock(&hal_mutex);
}