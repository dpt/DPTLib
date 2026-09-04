/* wuss/nearest-colour.c -- wuss - minimal window manager */

#include <assert.h>

#include "framebuf/pixelfmt.h"

#include "impl.h"

wuss_colour_t wuss_nearest_colour(const wuss_t *wuss, int r, int g, int b)
{
  unsigned long best_d;
  wuss_colour_t best_i;
  int           i;

  assert(wuss != NULL);
  assert(wuss->npalette > 0);

  best_d = ~0UL;
  best_i = 0;
  for (i = 0; i < wuss->npalette; i++)
  {
    unsigned int  px;
    long          dr, dg, db;
    unsigned long d;

    /* rgba8888 is 0xAABBGGRR (see pixelfmt.h). */
    px = wuss->palette[i].primary;
    dr = r - (long) ( px        & 0xFF);
    dg = g - (long) ((px >>  8) & 0xFF);
    db = b - (long) ((px >> 16) & 0xFF);
    d  = (unsigned long) (dr * dr + dg * dg + db * db);
    if (d < best_d)
    {
      best_d = d;
      best_i = i;
    }
  }

  return best_i;
}
