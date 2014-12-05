/* ceillog2.x -- returns the ceiling log2 of the argument */

#include "utils/barith.h"

#include "util.h"

unsigned int ceillog2(unsigned int x)
{
  unsigned int y;

  y = !ispower2(x); /* 1 if x is not a power of two, 0 otherwise */
  SPREADMSB(x);

  return countbits(x >> 1) + y;
}

