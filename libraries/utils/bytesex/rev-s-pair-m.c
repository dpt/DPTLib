/* rev-s-pair-m.c -- reversing bytesex */


#include "utils/bytesex.h"

unsigned int rev_s_pair_m(const unsigned char *p)
{
  unsigned int a, b, c, d;

  a = p[0];
  b = p[1];
  c = p[2];
  d = p[3];

  return (a << 8) | (b << 0) | (c << 24) | (d << 16);
}
