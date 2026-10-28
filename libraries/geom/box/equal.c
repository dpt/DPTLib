/* geom/box/equal.c -- return whether two boxes are the same */

#include "geom/box.h"

int box_equal(const box_t *a, const box_t *b)
{
  return a->x0 == b->x0 && a->y0 == b->y0 &&
         a->x1 == b->x1 && a->y1 == b->y1;
}
