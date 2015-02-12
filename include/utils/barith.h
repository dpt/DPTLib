/* barith.h -- binary arithmetic */

#ifndef UTILS_BARITH_H
#define UTILS_BARITH_H

#include <limits.h>
#include <stdint.h>

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
BARITH_INLINE int countbits_32(uint32_t);
BARITH_INLINE int countbits_64(uint64_t);

/* Returns the count of leading zeros of the argument. */
BARITH_INLINE int clz_32(uint32_t);
BARITH_INLINE int clz_64(uint64_t);

/* Count leading zeroes for bytes. */
BARITH_INLINE int clz8(unsigned int);

/* Returns the count of trailing zeros of the argument. */
BARITH_INLINE int ctz_32(uint32_t);
BARITH_INLINE int ctz_64(uint64_t);

#else

#define countbits_32(x)  __builtin_popcount(x)
#define countbits_64(x)  __builtin_popcountll(x)

#define clz_32(x)        __builtin_clz(x)
#define clz_64(x)        __builtin_clzll(x)

#define clz8(byte)       (byte ? __builtin_clz(byte) - 24 : 8)

#define ctz_32(x)        __builtin_ctz(x)
#define ctz_64(x)        __builtin_ctzll(x)

#endif

/* Returns the most significant set bit. */
BARITH_INLINE uint32_t msb_32(uint32_t);
BARITH_INLINE uint64_t msb_64(uint64_t);

/* Returns the power of two less than or equal to the argument. */
BARITH_INLINE uint32_t power2le_32(uint32_t);
BARITH_INLINE uint64_t power2le_64(uint64_t);

/* Returns the power of two greater than the argument. */
BARITH_INLINE uint32_t power2gt_32(uint32_t);
BARITH_INLINE uint64_t power2gt_64(uint64_t);

/* Returns the floor log2 of the argument. */
BARITH_INLINE uint32_t floorlog2_32(uint32_t);
BARITH_INLINE uint64_t floorlog2_64(uint64_t);

/* Returns the ceiling log2 of the argument. */
BARITH_INLINE uint32_t ceillog2_32(uint32_t);
BARITH_INLINE uint64_t ceillog2_64(uint64_t);

/* Returns the argument reversed bitwise. */
BARITH_INLINE uint32_t reversebits_32(uint32_t);
BARITH_INLINE uint64_t reversebits_64(uint64_t);

/* ----------------------------------------------------------------------- */

/* Spread the most significant set bit downwards so it fills all lower bits.
 */
#define SPREADMSB_32(x) \
x |= x >> 1; x |= x >> 2; x |= x >> 4; x |= x >> 8; x |= x >> 16
#define SPREADMSB_64(x) \
x |= x >> 1; x |= x >> 2; x |= x >> 4; x |= x >> 8; x |= x >> 16; x |= x >> 32

#ifndef BARITH_HAVE_GCC_BUILTINS
/* Returns the number of bits set in the argument. */
/* See http://en.wikipedia.org/wiki/Hamming_weight */
BARITH_INLINE int countbits_32(uint32_t x)
{
  x -=  (x >> 1) & 0x55555555;
  x  = ((x >> 2) & 0x33333333) + (x & 0x33333333);
  x  = ((x >> 4) + x) & 0x0f0f0f0f;
  x +=   x >> 8;
  x +=   x >> 16;

  return x & 0x3f;
}
BARITH_INLINE int countbits_64(uint32_t x)
{
  x -=  (x >> 1) & 0x5555555555555555;
  x  = ((x >> 2) & 0x3333333333333333) + (x & 0x3333333333333333);
  x  = ((x >> 4) + x) & 0x0f0f0f0f0f0f0f0f;
  x +=   x >> 8;
  x +=   x >> 16;
  x +=   x >> 32;

  return x & 0x7f;
}
#endif

#ifndef BARITH_HAVE_GCC_BUILTINS
/* Returns the count of leading zeros of the argument. */
/* See http://en.wikipedia.org/wiki/Find_first_set */
BARITH_INLINE int clz_32(uint32_t x)
{
#ifdef UNUSED_ALTERNATIVE
  SPREADMSB_32(x);
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

  SPREADMSB_32(x);
  return tab[(0x07dcd629 * (x + 1)) >> 27];
#endif
}
BARITH_INLINE int clz_64(uint64_t x)
{
  SPREADMSB_64(x);
  return 64 - countbits(x);
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
BARITH_INLINE int ctz_32(uint32_t x)
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
BARITH_INLINE int ctz_64(uint64_t x)
{
  return countbits(LSB(x) - 1);
}
#endif

/* Returns the most significant set bit. */
BARITH_INLINE uint32_t msb_32(uint32_t x)
{
  SPREADMSB_32(x);
  return x & ~(x >> 1);
}
BARITH_INLINE uint64_t msb_64(uint64_t x)
{
  SPREADMSB_64(x);
  return x & ~(x >> 1);
}

/* Returns the power of two less than or equal to the argument. */
BARITH_INLINE uint32_t power2le_32(uint32_t x)
{
  SPREADMSB_32(x);
  return (x >> 1) + 1;
}
BARITH_INLINE uint64_t power2le_64(uint64_t x)
{
  SPREADMSB_64(x);
  return (x >> 1) + 1;
}

/* Returns the power of two greater than the argument. */
BARITH_INLINE uint32_t power2gt_32(uint32_t x)
{
  SPREADMSB_32(x);
  return x + 1;
}
BARITH_INLINE uint64_t power2gt_64(uint64_t x)
{
  SPREADMSB_64(x);
  return x + 1;
}

/* Returns the floor log2 of the argument. */
BARITH_INLINE uint32_t floorlog2_32(uint32_t x)
{
  SPREADMSB_32(x);
  return countbits_32(x >> 1);
}
BARITH_INLINE uint64_t floorlog2_64(uint64_t x)
{
  SPREADMSB_64(x);
  return countbits_64(x >> 1);
}

/* Returns the ceiling log2 of the argument. */
BARITH_INLINE uint32_t ceillog2_32(uint32_t x)
{
  uint32_t y;

  y = !ISPOWER2(x); /* 1 if x is not a power of two, 0 otherwise */
  SPREADMSB_32(x);

  return countbits_32(x >> 1) + y;
}
BARITH_INLINE uint64_t ceillog2_64(uint64_t x)
{
  uint64_t y;

  y = !ISPOWER2(x); /* 1 if x is not a power of two, 0 otherwise */
  SPREADMSB_64(x);

  return countbits_64(x >> 1) + y;
}

/* Returns the argument reversed bitwise. */
BARITH_INLINE uint32_t reversebits_32(uint32_t x)
{
  uint32_t y;

  y = 0x55555555; x = ((x >> 1) & y) | ((x & y) << 1);
  y = 0x33333333; x = ((x >> 2) & y) | ((x & y) << 2);
  y = 0x0f0f0f0f; x = ((x >> 4) & y) | ((x & y) << 4);
  y = 0x00ff00ff; x = ((x >> 8) & y) | ((x & y) << 8);

  return (x >> 16) | (x << 16);
}
BARITH_INLINE uint64_t reversebits_64(uint64_t x)
{
  uint64_t y;

  y = 0x5555555555555555; x = ((x >>  1) & y) | ((x & y)  << 1);
  y = 0x3333333333333333; x = ((x >>  2) & y) | ((x & y)  << 2);
  y = 0x0f0f0f0f0f0f0f0f; x = ((x >>  4) & y) | ((x & y)  << 4);
  y = 0x00ff00ff00ff00ff; x = ((x >>  8) & y) | ((x & y)  << 8);
  y = 0x0000ffff0000ffff; x = ((x >> 16) & y) | ((x & y) << 16);

  return (x >> 32) | (x << 32);
}

/* ----------------------------------------------------------------------- */

/* */

#if UINT_MAX == 4294967295U

#define countbits     countbits_32
#define clz           clz_32
#define ctz           ctz_32
#define msb           msb_32
#define power2le      power2le_32
#define power2gt      power2gt_32
#define floorlog2     floorlog2_32
#define ceillog2      ceillog2_32
#define reversebits   reversebits_32

#elif UINT_MAX == 18446744073709551615ULL

#define countbits     countbits_64
#define clz           clz_64
#define ctz           ctz_64
#define msb           msb_64
#define power2le      power2le_64
#define power2gt      power2gt_64
#define floorlog2     floorlog2_64
#define ceillog2      ceillog2_64
#define reversebits   reversebits_64

#else

#error Unable to determine size of int.

#endif


#if SIZE_MAX == 4294967295U

#define countbits_size_t     countbits_32
#define clz_size_t           clz_32
#define ctz_size_t           ctz_32
#define msb_size_t           msb_32
#define power2le_size_t      power2le_32
#define power2gt_size_t      power2gt_32
#define floorlog2_size_t     floorlog2_32
#define ceillog2_size_t      ceillog2_32
#define reversebits_size_t   reversebits_32

#elif SIZE_MAX == 18446744073709551615ULL

#define countbits_size_t     countbits_64
#define clz_size_t           clz_64
#define ctz_size_t           ctz_64
#define msb_size_t           msb_64
#define power2le_size_t      power2le_64
#define power2gt_size_t      power2gt_64
#define floorlog2_size_t     floorlog2_64
#define ceillog2_size_t      ceillog2_64
#define reversebits_size_t   reversebits_64

#else

#error Unable to determine size of size_t.

#endif


// TODO: Add variants for other types here.


#endif /* UTILS_BARITH_H */
