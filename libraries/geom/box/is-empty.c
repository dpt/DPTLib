/* is-empty.c -- return whether the specified box is empty */

#include "geom/box.h"

int box_is_empty(const box_t *a)
{
  return (a->x0 >= a->x1) ||
         (a->y0 >= a->y1);
}
