/* msb.c -- returns the most significant set bit */

#include "utils/barith.h"

#include "util.h"

unsigned int msb(unsigned int x)
{
  SPREADMSB(x);
  return x & ~(x >> 1);
}
