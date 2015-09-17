/* reset.c -- set box 'b' to be invalid */

#include <limits.h>

#include "geom/box.h"

void box_reset(box_t *b)
{
  const box_t invalid = { INT_MAX, INT_MAX, INT_MIN, INT_MIN };

  *b = invalid;
}
