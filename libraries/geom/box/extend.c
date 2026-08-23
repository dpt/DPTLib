/* extend.c -- extend box "b" to include (x,y). */

#include <stdarg.h>

#include "base/utils.h"

#include "geom/box.h"

void box_extend(box_t *b, int x, int y)
{
  b->x0 = MIN(b->x0, x + 0);
  b->y0 = MIN(b->y0, y + 0);
  b->x1 = MAX(b->x1, x + 1);
  b->y1 = MAX(b->y1, y + 1);
}

void box_extend_n(box_t *b, int npoints, ...)
{
  va_list ap;
  int     i;

  va_start(ap, npoints);

  for (i = 0; i < npoints; i++)
  {
    int x,y;

    x = va_arg(ap, int);
    y = va_arg(ap, int);

    box_extend(b, x, y);
  }

  va_end(ap);
}
