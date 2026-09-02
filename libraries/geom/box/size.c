/* geom/box/size.c -- return the size of the specified box */

#include "geom/box.h"
#include "geom/size.h"

size2d_t box_size(const box_t *box)
{
  size2d_t size;

  size.w = box->x1 - box->x0;
  size.h = box->y1 - box->y0;

  return size;
}
