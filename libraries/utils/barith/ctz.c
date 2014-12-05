/* ctz.c -- returns the count of trailing zeros of the argument */

#include "utils/barith.h"

#include "util.h"

int ctz(unsigned int x)
{
#ifdef UNUSED_ALTERNATIVE
  return countbits(lsb(x) - 1);
#else
  static const unsigned char tab[32] = { 31,  0, 22,  1, 28, 23, 13,  2,
                                         29, 26, 24, 17, 19, 14,  9,  3,
                                         30, 21, 27, 12, 25, 16, 18,  8,
                                         20, 11, 15,  7, 10,  6,  5,  4 };

  x = lsb(x);
  if (x == 0)
    return 32;

  return tab[(0x0fb9ac52 * x) >> 27];
#endif
}
