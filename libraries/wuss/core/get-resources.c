/* wuss/get-resources.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

const char *wuss_get_resources(const wuss_t *wuss)
{
  assert(wuss != NULL);

  return wuss->resources;
}
