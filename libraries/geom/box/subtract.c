/* geom/box/subtract.c -- split box "a" minus box "b" into up to four bands */

#include "geom/box.h"

static void band(box_t *out, int x0, int y0, int x1, int y1)
{
  out->x0 = x0;
  out->y0 = y0;
  out->x1 = x1;
  out->y1 = y1;
}

int box_subtract(const box_t *a, const box_t *b, box_t out[4])
{
  box_t cut;
  int   n;

  if (box_intersection(a, b, &cut))
  {
    out[0] = *a; /* no overlap: all of "a" survives */
    return 1;
  }

  n = 0;
  if (cut.y0 > a->y0)
    band(&out[n++], a->x0, a->y0, a->x1, cut.y0); /* above */
  if (cut.y1 < a->y1)
    band(&out[n++], a->x0, cut.y1, a->x1, a->y1); /* below */
  if (cut.x0 > a->x0)
    band(&out[n++], a->x0, cut.y0, cut.x0, cut.y1); /* left */
  if (cut.x1 < a->x1)
    band(&out[n++], cut.x1, cut.y0, a->x1, cut.y1); /* right */

  return n;
}
