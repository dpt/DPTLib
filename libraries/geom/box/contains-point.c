/* contains-point.c -- return true if "box" contains the point (x,y) */

#include "base/utils.h"

#include "geom/box.h"

int box_contains_point(const box_t *box, int x, int y)
{
  return x >= box->x0 && x <= box->x1 &&
         y >= box->y0 && y <= box->y1;
}
