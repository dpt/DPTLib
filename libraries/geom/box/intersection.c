/* intersection.c -- compute "c" the result of intersecting boxes "a" and "b" */

#include "base/utils.h"

#include "geom/box.h"

int box_intersection(const box_t *a, const box_t *b, box_t *c)
{
  c->x0 = MAX(a->x0, b->x0);
  c->y0 = MAX(a->y0, b->y0);
  c->x1 = MIN(a->x1, b->x1);
  c->y1 = MIN(a->y1, b->y1);

  return box_is_empty(c);
}
