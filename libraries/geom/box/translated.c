/* translated.c -- translate box "b" by (x,y) producing new box "t" */

#include "base/utils.h"

#include "geom/box.h"

void box_translated(const box_t *b, int x, int y, box_t *t)
{
  t->x0 = b->x0 + x;
  t->y0 = b->y0 + y;
  t->x1 = b->x1 + x;
  t->y1 = b->y1 + y;
}
