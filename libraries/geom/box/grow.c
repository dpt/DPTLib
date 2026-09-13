/* geom/box/grow.c -- increases the size of "box" by "change" */

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

box_t box_grown(const box_t *b, int change)
{
  box_t result;
  
  result.x0 = b->x0 - change;
  result.y0 = b->y0 - change;
  result.x1 = b->x1 + change;
  result.y1 = b->y1 + change;
  
  return result;
}
