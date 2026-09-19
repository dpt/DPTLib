/* wuss/get-screen-size.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

size2d_t wuss_get_screen_size(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return wuss->scr->size;
}
