/* rev-s-pair.c -- reversing bytesex */

#include "utils/bytesex.h"

unsigned int rev_s_pair(unsigned int r0)
{
  unsigned int mask;
  unsigned int r1;

  mask = 0xff00ffffU;

  r1  = mask & (r0 << 8);
  r0 &= ~(mask >> 8);
  r0  = r1 | (r0 >> 8);

  return r0;
}
