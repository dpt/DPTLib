/* could-hold.c -- return true if "box" can hold a box of size (w,h) */

#include "geom/box.h"

int box_could_hold(const box_t *box, int w, int h)
{
  return (box->x1 - box->x0) >= w &&
         (box->y1 - box->y0) >= h;
}
