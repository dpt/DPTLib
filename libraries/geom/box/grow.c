/* grow.c -- increases the size of "box" by "change" */

#include "geom/box.h"

void box_grow(box_t *box, int change)
{
  box->x0 -= change;
  box->y0 -= change;
  box->x1 += change;
  box->y1 += change;
}
