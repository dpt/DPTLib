/* power2le.c -- returns the power of two less than or equal to the arg */

#include "utils/barith.h"

#include "util.h"

unsigned int power2le(unsigned int x)
{
  SPREADMSB(x);
  return (x >> 1) + 1;
}
