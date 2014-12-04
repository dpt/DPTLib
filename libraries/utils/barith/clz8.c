/* clz8.c -- count leading zeroes in a byte */

#include "utils/barith.h"

#ifndef BARITH_HAVE_GCC_CLZ

int clz8(unsigned int byte)
{
  static const unsigned char counts[32] =
  {
    8, 7, 6, 6, 5, 5, 5, 5,
    4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3,
  };

  if (byte & 0x80) return 0;
  if (byte & 0x40) return 1;
  if (byte & 0x20) return 2;

  return counts[byte];
}

#else

extern int dummy;

#endif
