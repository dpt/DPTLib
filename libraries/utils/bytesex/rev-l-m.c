/* rev-l-m.c -- reversing bytesex */

#include "utils/bytesex.h"

unsigned int rev_l_m(const unsigned char *p)
{
  unsigned int a, b, c, d;

  a = p[0];
  b = p[1];
  c = p[2];
  d = p[3];

  return (a << 24) | (b << 16) | (c << 8) | (d << 0);
}
