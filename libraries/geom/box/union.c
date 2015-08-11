/* union.c -- return a box "c" that contains both boxes "a" and "b" */

#include "base/utils.h"

#include "geom/box.h"

void box_union(const box_t *a, const box_t *b, box_t *c)
{
  c->x0 = MIN(a->x0, b->x0);
  c->y0 = MIN(a->y0, b->y0);
  c->x1 = MAX(a->x1, b->x1);
  c->y1 = MAX(a->y1, b->y1);
}
