/* clz.c -- returns the count of leading zeros of the argument */

#include "utils/barith.h"

#include "util.h"

int clz(unsigned int x)
{
#ifdef UNUSED_ALTERNATIVE
  SPREADMSB(x);
  return 32 - countbits(x);
#else
  /*
   * Source:
   *
   * From:       Richard Woodruff <richardw@microware.com>
   * Newsgroups: comp.sys.arm
   * Subject:    Re: Finding highest/lowest set bit
   * Date:       28 Oct 2000 01:01:26 GMT
   */

  static const unsigned char tab[32] = { 0, 31,  9, 30,  3,  8, 18, 29,
                                         2,  5,  7, 14, 12, 17, 22, 28,
                                         1, 10,  4, 19,  6, 15, 13, 23,
                                        11, 20, 16, 24, 21, 25, 26, 27 };

  if (x == 0)
    return 32;

  SPREADMSB(x);
  return tab[(0x07dcd629 * (x + 1)) >> 27];
#endif
}
