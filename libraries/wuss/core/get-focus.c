/* wuss/get-focus.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

wuss_window_t *wuss_get_focus(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return wuss->focus;
}
