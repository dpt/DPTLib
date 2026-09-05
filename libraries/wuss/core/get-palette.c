/* wuss/get-palette.c -- wuss - minimal window manager */

#include <assert.h>

#include "impl.h"

const colour_t *wuss_get_palette(const wuss_t *wuss, int *npalette)
{
  assert(wuss != NULL);

  if (npalette != NULL)
    *npalette = wuss->npalette;

  return wuss->palette;
}
