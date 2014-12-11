/* round.c -- round a box's coords so they're a multiple of "amount" */

#include "utils/minmax.h"

#include "geom/box.h"

void box_round(box_t *box, int amount)
{
  // not fully tested
  box->x0 = ((box->x0 - (amount - 1)) / amount) * amount;
  box->y0 = ((box->y0 - (amount - 1)) / amount) * amount;
  box->x1 = ((box->x1 + (amount - 1)) / amount) * amount;
  box->y1 = ((box->y1 + (amount - 1)) / amount) * amount;
}
