/* rev-l.c -- reversing bytesex */

#include "utils/bytesex.h"

#include "util.h"

unsigned int rev_l(unsigned int r0)
{
  unsigned int mask;
  unsigned int r1;

  mask = 0xffff00ffU;

  r1 = r0 ^ ROR(int, r0, 16);
  r1 = mask & (r1 >> 8);
  r0 = r1 ^ ROR(int, r0, 8);

  return r0;
}
