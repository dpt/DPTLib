/* floorlog2.c -- returns the floor log2 of the argument */

#include "utils/barith.h"

#include "util.h"

unsigned int floorlog2(unsigned int x)
{
  SPREADMSB(x);
  return countbits(x >> 1);
}
