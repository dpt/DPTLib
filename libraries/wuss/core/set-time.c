/* wuss/set-time.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

void wuss_set_time(wuss_t *wuss, unsigned int ms)
{
  assert(wuss != NULL);

  wuss->now_ms = ms;
}
