/* get-pointer.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

point_t wuss_get_pointer(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return wuss->pointer;
}
