/* barith.h -- binary arithmetic */

#ifndef UTILS_BARITH_H
#define UTILS_BARITH_H

/* Returns the least significant set bit.
 * Graphics Gems II: 'Bit Picking', p366.
 */
#define lsb(x) ((x) & (0 - (x))) /* 0-x rather than -x to avoid a warning */

/* x &= x - 1 clears the LSB */

/* Returns the most significant set bit. */
unsigned int msb(unsigned int);

/* Returns the number of bits set in the argument. */
int countbits(unsigned int);

/* Returns the count of leading zeros of the argument. */
int clz(unsigned int);

/* Returns the count of trailing zeros of the argument. */
int ctz(unsigned int);

/* Returns the power of two less than or equal to the argument. */
unsigned int power2le(unsigned int);

/* Returns the power of two greater than the argument. */
unsigned int power2gt(unsigned int);

/* Returns whether the argument is a power of 2. */
#define ispower2(x) (((x) & ((x) - 1)) == 0)

/* Returns the floor log2 of the argument. */
unsigned int floorlog2(unsigned int);

/* Returns the ceiling log2 of the argument. */
unsigned int ceillog2(unsigned int);

/* Returns the argument reversed bitwise. */
unsigned int reversebits(unsigned int);

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
