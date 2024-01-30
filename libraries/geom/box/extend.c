/* extend.c -- extend box "b" to include (x,y). */

#include "base/utils.h"

#include "geom/box.h"

void box_extend(box_t *b, int x, int y)
{
  b->x0 = MIN(b->x0, x + 0);
  b->y0 = MIN(b->y0, y + 0);
  b->x1 = MAX(b->x1, x + 1);
  b->y1 = MAX(b->y1, y + 1);
}
