/* contains-box.c -- return true if box "inside" is contained by box "outside" */

#include "geom/box.h"

int box_contains_box(const box_t *inner, const box_t *outer)
{
  return inner->x0 >= outer->x0 &&
         inner->y0 >= outer->y0 &&
         inner->x1 <= outer->x1 &&
         inner->y1 <= outer->y1;
}
