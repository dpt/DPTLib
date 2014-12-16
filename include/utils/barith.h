/* barith.h -- binary arithmetic */

#ifndef UTILS_BARITH_H
#define UTILS_BARITH_H

/* ----------------------------------------------------------------------- */

/* The inline arrangement here is taken from "Inline Functions In C"
 * http://www.greenend.org.uk/rjk/tech/inline.html
 *
 * If BARITH_INLINE is defined then we'll generate inline functions,
 * otherwise we'll generate ordinary functions.
 *
 * We generate inline functions, but we also need functions declared for when
 * the inline form is not chosen by the compiler.
 */

#ifndef BARITH_INLINE
#  if __GNUC__ && !__GNUC_STDC_INLINE__
#    define BARITH_INLINE extern __inline__
#  else
#    define BARITH_INLINE __inline__
#  endif
#endif

/* Test to see if we can use __builtin_popcount, __builtin_clz and
 * __builtin_ctz. */
#if defined(__GNUC__) && __GNUC__ >= 4 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define BARITH_HAVE_GCC_BUILTINS
#endif

/* ----------------------------------------------------------------------- */

/* Returns the least significant set bit.
 * Graphics Gems II: 'Bit Picking', p366.
 */
#define LSB(x) ((x) & (0 - (x))) /* 0-x rather than -x to avoid a warning */

/* x &= x - 1 clears the LSB */

/* Returns whether the argument is a power of 2. */
#define ISPOWER2(x) (((x) & ((x) - 1)) == 0)

/* Decode a "zero means 2^N" field. */
#define ZEROIS2N(x, n) ((((x) - 1) & ((1 << (n)) - 1)) + 1)

/* ----------------------------------------------------------------------- */

#ifndef BARITH_HAVE_GCC_BUILTINS

/* Returns the number of bits set in the argument. */
BARITH_INLINE int countbits(unsigned int);

/* Returns the count of leading zeros of the argument. */
BARITH_INLINE int clz(unsigned int);

/* Count leading zeroes for bytes. */
BARITH_INLINE int clz8(unsigned int);

/* Returns the count of trailing zeros of the argument. */
BARITH_INLINE int ctz(unsigned int);

#else

#define countbits(x)  __builtin_popcount(x)

#define clz(x)        __builtin_clz(x)

#define clz8(byte)    (byte ? __builtin_clz(byte) - 24 : 8)

#define ctz(x)        __builtin_ctz(x)

#endif

/* Returns the most significant set bit. */
BARITH_INLINE unsigned int msb(unsigned int);

/* Returns the power of two less than or equal to the argument. */
BARITH_INLINE unsigned int power2le(unsigned int);

/* Returns the power of two greater than the argument. */
BARITH_INLINE unsigned int power2gt(unsigned int);

/* Returns the floor log2 of the argument. */
BARITH_INLINE unsigned int floorlog2(unsigned int);

/* Returns the ceiling log2 of the argument. */
BARITH_INLINE unsigned int ceillog2(unsigned int);

/* Returns the argument reversed bitwise. */
BARITH_INLINE unsigned int reversebits(unsigned int);

/* ----------------------------------------------------------------------- */

/* Spread the most significant set bit downwards so it fills all lower bits.
 */
#define SPREADMSB(x) \
  x |= x >> 1; x |= x >> 2; x |= x >> 4; x |= x >> 8; x |= x >> 16

#ifndef BARITH_HAVE_GCC_BUILTINS
/* Returns the number of bits set in the argument. */
BARITH_INLINE int countbits(unsigned int x)
{
  x -=  (x >> 1) & 0x55555555;
  x  = ((x >> 2) & 0x33333333) + (x & 0x33333333);
  x  = ((x >> 4) + x) & 0x0f0f0f0f;
  x +=   x >> 8;
  x +=   x >> 16;
  
  return x & 0x3f;
}
#endif

#ifndef BARITH_HAVE_GCC_BUILTINS
/* Returns the count of leading zeros of the argument. */
BARITH_INLINE int clz(unsigned int x)
{
#ifdef UNUSED_ALTERNATIVE
  SPREADMSB(x);
  return 32 - countbits(x);
#else
  /* Source:
   *
   * From:       Richard Woodruff <richardw@microware.com>
   * Newsgroups: comp.sys.arm
   * Subject:    Re: Finding highest/lowest set bit
   * Date:       28 Oct 2000 01:01:26 GMT
   */
  
  static const unsigned char tab[32] =
  {
    0, 31,  9, 30,  3,  8, 18, 29,
    2,  5,  7, 14, 12, 17, 22, 28,
    1, 10,  4, 19,  6, 15, 13, 23,
   11, 20, 16, 24, 21, 25, 26, 27
  };
  
  if (x == 0)
    return 32;
  
  SPREADMSB(x);
  return tab[(0x07dcd629 * (x + 1)) >> 27];
#endif
}
#endif

#ifndef BARITH_HAVE_GCC_BUILTINS
/* Count leading zeroes for bytes. */
BARITH_INLINE int clz8(unsigned int byte)
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
#endif

#ifndef BARITH_HAVE_GCC_BUILTINS
/* Returns the count of trailing zeros of the argument. */
BARITH_INLINE int ctz(unsigned int x)
{
#ifdef UNUSED_ALTERNATIVE
  return countbits(LSB(x) - 1);
#else
  static const unsigned char tab[32] =
  {
    31,  0, 22,  1, 28, 23, 13,  2,
    29, 26, 24, 17, 19, 14,  9,  3,
    30, 21, 27, 12, 25, 16, 18,  8,
    20, 11, 15,  7, 10,  6,  5,  4
  };
  
  x = LSB(x);
  if (x == 0)
    return 32;
  
  return tab[(0x0fb9ac52 * x) >> 27];
#endif
}
#endif

/* Returns the most significant set bit. */
BARITH_INLINE unsigned int msb(unsigned int x)
{
  SPREADMSB(x);
  return x & ~(x >> 1);
}

/* Returns the power of two less than or equal to the argument. */
BARITH_INLINE unsigned int power2le(unsigned int x)
{
  SPREADMSB(x);
  return (x >> 1) + 1;
}

/* Returns the power of two greater than the argument. */
BARITH_INLINE unsigned int power2gt(unsigned int x)
{
  SPREADMSB(x);
  return x + 1;
}

/* Returns the floor log2 of the argument. */
BARITH_INLINE unsigned int floorlog2(unsigned int x)
{
  SPREADMSB(x);
  return countbits(x >> 1);
}

/* Returns the ceiling log2 of the argument. */
BARITH_INLINE unsigned int ceillog2(unsigned int x)
{
  unsigned int y;
  
  y = !ISPOWER2(x); /* 1 if x is not a power of two, 0 otherwise */
  SPREADMSB(x);
  
  return countbits(x >> 1) + y;
}

/* Returns the argument reversed bitwise. */
BARITH_INLINE unsigned int reversebits(unsigned int x)
{
  unsigned int y;
  
  y = 0x55555555; x = ((x >> 1) & y) | ((x & y) << 1);
  y = 0x33333333; x = ((x >> 2) & y) | ((x & y) << 2);
  y = 0x0f0f0f0f; x = ((x >> 4) & y) | ((x & y) << 4);
  y = 0x00ff00ff; x = ((x >> 8) & y) | ((x & y) << 8);
  
  return (x >> 16) | (x << 16);
}

/* ----------------------------------------------------------------------- */

#endif /* UTILS_BARITH_H */
