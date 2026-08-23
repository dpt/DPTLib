/* grow.c -- increases the size of "box" by "change" */

#include "geom/box.h"

void box_scalelog2(box_t *b, int log2scale)
{
  b->x0 <<= log2scale;
  b->y0 <<= log2scale;
  b->x1 <<= log2scale;
  b->y1 <<= log2scale;
}

void box_grow(box_t *box, int change)
{
  box->x0 -= change;
  box->y0 -= change;
  box->x1 += change;
  box->y1 += change;
}
