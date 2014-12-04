/* barith.h -- binary arithmetic */

#ifndef UTILS_BARITH_H
#define UTILS_BARITH_H

#if defined(__GNUC__) && __GNUC__ >= 4 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define BARITH_HAVE_GCC_CLZ
#endif

/* Count leading zeroes for bytes. */
#ifdef BARITH_HAVE_GCC_CLZ
#define clz8(byte) (byte ? __builtin_clz(byte) - 24 : 8)
#else
int clz8(unsigned int byte);
#endif

/* Decode a "zero means 2^N" field. */
#define ZEROIS2N(x, n) ((((x) - 1) & ((1 << (n)) - 1)) + 1)

#endif /* UTILS_BARITH_H */
