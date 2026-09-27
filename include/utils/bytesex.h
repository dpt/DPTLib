/* utils/bytesex.h -- reversing bytesex */

#ifndef UTILS_BYTESEX_H
#define UTILS_BYTESEX_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

/* ----------------------------------------------------------------------- */

/* The inline arrangement here follows utils/barith.h.
 *
 * If BYTESEX_INLINE is defined then we'll generate inline functions,
 * otherwise we'll generate ordinary functions. utils/bytesex/bytesex.c
 * supplies the out-of-line definitions.
 */

#ifndef BYTESEX_INLINE
#  if __GNUC__ && !__GNUC_STDC_INLINE__
#    define BYTESEX_INLINE extern __inline__
#  elif defined(_MSC_VER)
#    define BYTESEX_INLINE __inline
#  else
#    define BYTESEX_INLINE __inline__
#  endif
#endif

/* ----------------------------------------------------------------------- */

/* Reverse bytesex.
 *
 * 's'hort or 'l'ong
 *
 * _m -> from memory (takes a pointer, reads as chars)
 * _pair -> two packed arguments
 * _block -> array of things to swap
 */

BYTESEX_INLINE unsigned short int rev_s(unsigned short int);
BYTESEX_INLINE unsigned short int rev_s_m(const unsigned char *);

BYTESEX_INLINE unsigned int rev_s_pair(unsigned int);
BYTESEX_INLINE unsigned int rev_s_pair_m(const unsigned char *);

BYTESEX_INLINE unsigned int rev_l(unsigned int);
BYTESEX_INLINE unsigned int rev_l_m(const unsigned char *);

void rev_s_block(unsigned short int *array, size_t nelems);
void rev_l_block(unsigned int       *array, size_t nelems);

/* ----------------------------------------------------------------------- */

BYTESEX_INLINE unsigned short int rev_s(unsigned short int r0)
{
  return (unsigned short int)((r0 >> 8) | (r0 << 8));
}

BYTESEX_INLINE unsigned short int rev_s_m(const unsigned char *p)
{
  unsigned short a, b;

  a = p[0];
  b = p[1];

  return (unsigned short int)((a << 8) | (b << 0));
}

BYTESEX_INLINE unsigned int rev_s_pair(unsigned int r0)
{
  unsigned int mask;
  unsigned int r1;

  mask = 0xff00ffffU;

  r1  = mask & (r0 << 8);
  r0 &= ~(mask >> 8);
  r0  = r1 | (r0 >> 8);

  return r0;
}

BYTESEX_INLINE unsigned int rev_s_pair_m(const unsigned char *p)
{
  unsigned int a, b, c, d;

  a = p[0];
  b = p[1];
  c = p[2];
  d = p[3];

  return (a << 8) | (b << 0) | (c << 24) | (d << 16);
}

BYTESEX_INLINE unsigned int rev_l(unsigned int r0)
{
  unsigned int mask;
  unsigned int r1;

  mask = 0xffff00ffU;

  /* r1 = r0 ^ ROR(r0, 16), then r0 = r1 ^ ROR(r0, 8) */
  r1 = r0 ^ ((r0 >> 16) | (r0 << 16));
  r1 = mask & (r1 >> 8);
  r0 = r1 ^ ((r0 >> 8) | (r0 << 24));

  return r0;
}

BYTESEX_INLINE unsigned int rev_l_m(const unsigned char *p)
{
  unsigned int a, b, c, d;

  a = p[0];
  b = p[1];
  c = p[2];
  d = p[3];

  return (a << 24) | (b << 16) | (c << 8) | (d << 0);
}

#ifdef __cplusplus
}
#endif

#endif /* UTILS_BYTESEX_H */
