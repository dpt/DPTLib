/* utils/maths.h -- maths utils */

#ifndef UTILS_MATHS_H
#define UTILS_MATHS_H

#include "base/utils.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Greatest common divisor of m,n */
int gcd(int m, int n);

/* Degrees to radians */
double degs_to_rads(double degs);

/* The reciprocal div_recip wants for divisor d, d > 0. It is a constant
 * expression when d is one. */
#define DIV_RECIP(d) (0xFFFFFFFFu / (d))

/* n / d, rounded down, for d > 0 and n < 2^31, given r = DIV_RECIP(d).
 * n * r / 2^32 falls short of n / d by less than one, so the estimate wants
 * at most one step up. Where many values share a divisor, this trades a
 * division for a multiply, which pays on targets without a hardware
 * divide. */
static INLINE unsigned int div_recip(unsigned int n,
                                     unsigned int d,
                                     unsigned int r)
{
  unsigned int q;

  q = (unsigned int) (((unsigned long long) n * r) >> 32);
  if (n - q * d >= d)
    q++;

  return q;
}

/* x / 255, rounded to nearest, for x <= 65662 (covers any product of two
 * 8-bit values). Shifts and adds only, no division.
 *
 * See Blinn (1995) Dirty Pixels: Chapter 19: Three Wrongs Make A Right. */
static INLINE unsigned int div255(unsigned int x)
{
  x += 0x80;
  return (x + (x >> 8)) >> 8;
}

#ifdef __cplusplus
}
#endif

#endif /* UTILS_MATHS_H */
