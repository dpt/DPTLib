/* wuss/nearest-colour.c -- wuss - minimal window manager */

#include <assert.h>

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "impl.h"

wuss_colour_t wuss_nearest_colour(const wuss_t *wuss, int r, int g, int b)
{
  assert(wuss != NULL);
  assert(wuss->npalette > 0);

  return (wuss_colour_t) colour_to_pixel(wuss->palette,
                                         wuss->npalette,
                                         colour_rgb(r, g, b),
                                         pixelfmt_p8);
}
