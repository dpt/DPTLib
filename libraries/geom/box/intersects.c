/* intersects.c -- return true if box "a" overlaps box "b" */

#include "geom/box.h"

int box_intersects(const box_t *a, const box_t *b)
{
  return a->x0 < b->x1 && a->x1 > b->x0 &&
         a->y0 < b->y1 && a->y1 > b->y0;
}
