/* gcd.c -- greatest common divisor */

#include "utils/maths.h"

/* Knuth Volume 1 1.1E p2 */

int gcd(int m, int n)
{
  int r;

  while (n > 0)
  {
    r = m % n;
    m = n;
    n = r;
  }

  return m;
}
