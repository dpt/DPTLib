/* rev-s-m.c -- reversing bytesex */

#include "utils/bytesex.h"

unsigned short int rev_s_m(const unsigned char *p)
{
  unsigned short a, b;

  a = p[0];
  b = p[1];

  return (unsigned short int) ((a << 8) | (b << 0));
}
