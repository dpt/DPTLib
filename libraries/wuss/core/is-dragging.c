/* wuss/is-dragging.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

int wuss_is_dragging(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return wuss->drag_window != NULL;
}
