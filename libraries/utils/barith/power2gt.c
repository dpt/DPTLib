/* power2gt.c -- returns the power of two greater than the argument */

#include "utils/barith.h"

#include "util.h"

unsigned int power2gt(unsigned int x)
{
  SPREADMSB(x);
  return x + 1;
}
