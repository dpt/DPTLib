/* rev-s.c -- reversing bytesex */

#include "utils/bytesex.h"

unsigned short int rev_s(unsigned short int r0)
{
  return (unsigned short int) ((r0 >> 8) | (r0 << 8));
}
