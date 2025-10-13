#ifndef uMODEM_UTILS_H_
#define uMODEM_UTILS_H_

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>

#include "umodem_hal.h"

static inline void *umodem_memmem(const void *haystack, size_t haystacklen,
                                  const void *needle, size_t needlelen)
{
  if (needlelen == 0)
    return (void *)haystack;
  if (haystacklen < needlelen)
    return NULL;
  const unsigned char *h = (const unsigned char *)haystack;
  const unsigned char *n = (const unsigned char *)needle;
  const unsigned char first = n[0];
  size_t last = haystacklen - needlelen + 1;
  for (size_t i = 0; i < last; i++)
  {
    if (h[i] == first)
    {
      size_t j = 1;
      while (j < needlelen && h[i + j] == n[j])
        j++;
      if (j == needlelen)
        return (void *)(h + i);
    }
  }
  return NULL;
}

static inline int umodem_strtoi(const char *str, int min_val, int max_val, int *out)
{
  if (!str)
    return 0;
  char *endptr;
  errno = 0;
  long val = strtol(str, &endptr, 10);
  if (errno != 0 || endptr == str || *endptr != '\0')
    return 0; // invalid input
  if (val < min_val || val > max_val)
    return 0;
  *out = (int)val;
  return 1; // success
}

// Simple LCG (Linear Congruential Generator)
// Constants from glibc: a=1103515245, c=12345
static inline uint32_t umodem_rand(void)
{
  static int first_call = 1;
  static uint32_t state = 0;
  if (first_call)
  {
    int stack_var;
    state = umodem_hal_millis() ^ (uint32_t)(uintptr_t)&stack_var;
    first_call = 0;
  }
  state = state * 1103515245U + 12345U;
  return state;
}

#define UMODEM_MEMMEM(haystack, hlen, needle, needlelen) \
  umodem_memmem((haystack), (hlen), (needle), (needlelen))
#define UMODEM_STRTOI(str, min_val, max_val, out) \
  umodem_strtoi((str), (min_val), (max_val), (out))
#define UMODEM_RAND() umodem_rand()

#endif