/* clip.c -- compute "c" as the clip factors of box "b" over "a" */

#include "base/utils.h"

#include "geom/box.h"

void box_clip(const box_t *a, const box_t *b, box_t *c)
{
  c->x0 = MAX(a->x0 - b->x0, 0);
  c->y0 = MAX(a->y0 - b->y0, 0);
  c->x1 = MAX(b->x1 - a->x1, 0);
  c->y1 = MAX(b->y1 - a->y1, 0);
}
