/* is-empty.c -- return whether the specified box is empty */

#include "geom/box.h"

int box_is_empty(const box_t *box)
{
  return (box->x0 >= box->x1) ||
         (box->y0 >= box->y1);
}
