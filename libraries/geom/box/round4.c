/* round4.c -- round a box's coords so they're a multiple of 4 */

#include "geom/box.h"

void box_round4(box_t *box)
{
  box_t b = *box;

  b.x0 = (b.x0    ) & ~3;
  b.y0 = (b.y0    ) & ~3;
  b.x1 = (b.x1 + 3) & ~3;
  b.y1 = (b.y1 + 3) & ~3;

  *box = b;
}
