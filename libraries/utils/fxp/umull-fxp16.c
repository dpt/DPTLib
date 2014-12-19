/* umull-fxp16.c -- fixed point helpers */

#include "utils/fxp.h"

#define T  unsigned int
#define LL unsigned long long

T umull_fxp16(T x, T y)
{
  return (T)(((LL) x * (LL) y) >> 16);
}
