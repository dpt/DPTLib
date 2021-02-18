/* round.c -- round a box's coords so they're a multiple of log2 x,y */

#include "base/utils.h"

#include "geom/box.h"

void box_round(box_t *pb, int log2x, int log2y)
{
  const int x = (1 << log2x) - 1;
  const int y = (1 << log2y) - 1;
  box_t     b = *pb;

  b.x0 = (b.x0    ) & ~x;
  b.y0 = (b.y0    ) & ~y;
  b.x1 = (b.x1 + x) & ~x;
  b.y1 = (b.y1 + y) & ~y;

  *pb = b;
}
