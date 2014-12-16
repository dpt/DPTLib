/* bitarr.h -- arrays of bits */

/**
 * \file bitarr.h
 *
 * Arrays of bits.
 *
 * Bit arrays are an array of bits. They are of a fixed length and allocated
 * by the client. The bit array library provides functions to manipulate bit
 * arrays but not allocate them.
 *
 * \see Bit Vector for manipulating a dynamically allocated bit array.
 *
 * \warning The bit array functions performs no bounds checks.
 */

// TODO
//
// I started out with the idea that bit arrays should be allocated by number
// of bits, so if you wanted a 97-bit array you could have it. But for a
// 97-bit array you have three 32-bit words plus a single bit. By the time
// you're manipulating the bits then you've lost the original size of the
// array and you're operating in word-sized chunks so you can't tell if
// you're out of bounds.
//
// So we need to document that sizes get rounded up.

#ifndef DATASTRUCT_BITARR_H
#define DATASTRUCT_BITARR_H

#include <stdlib.h>

#include "utils/array.h" /* for 'UNKNOWN' */

#define T bitarr_t

/**
 * A bit array's type.
 */
typedef struct bitarr T;

/**
 * A bit array's element type.
 */
typedef unsigned int bitarr_elem_t;

#define BITARR_SHIFT 5 /* log2 (sizeof(bitarr_elem_t) * CHAR_BIT) */
#define BITARR_BITS (1u << BITARR_SHIFT)
#define BITARR_MASK (BITARR_BITS - 1)

/**
 * Return the number of elements required to store the specified number of
 * bits.
 */
#define BITARR_ELEMS(nbits) ((nbits + BITARR_BITS - 1) >> BITARR_SHIFT)

/**
 * Declare a bit array with the specified number of bits.
 *
 * This uses the OSLib trick of declaring a macro to define a type and also
 * declaring an 'equivalent' struct. In practice they're not actually
 * equivalent as the former, bitarr_ARRAY, is an anonymous struct of a given
 * length and the bitarr_t is a struct containing an array of 'UNKNOWN'
 * length, which is actually defined as 1.
 *
 * This works out all right, letting us declare bit arrays by specifying the
 * number of bits we require, but we end up needing to cast out declared
 * entries to (bitarr_t *) when calling a 'real' function, e.g. bitarr_count
 * which looks awkward.
 */
#define bitarr_ARRAY(nbits)                     \
  struct {                                      \
    bitarr_elem_t entries[BITARR_ELEMS(nbits)]; \
  }

/**
 * A bit array.
 */
struct bitarr
{
  bitarr_elem_t entries[UNKNOWN];
};

/**
 * Clear all of the bits in the specified bit array.
 *
 * \param arr     Bit array.
 * \param bytelen Byte length of the bit array.
 */
#define bitarr_wipe(arr, bytelen)       \
  do {                                  \
    memset(&(arr).entries, 0, bytelen); \
  } while (0)

/**
 * The 'BITARR_OP' macro implements the core operations.
 *
 * \param arr     Bit array.
 * \param bytelen Bit to operate on.
 * \param op      Logical operation such as |=, ^=, etc.
 */
#define BITARR_OP(arr, bit, op) \
  ((arr)->entries[(bit) >> BITARR_SHIFT] op (1u << ((bit) & BITARR_MASK)))

/**
 * Set a single bit.
 */
#define bitarr_set(arr, bit) \
  do { BITARR_OP(arr, bit, |=); } while (0)

/**
 * Clear a single bit.
 * */
#define bitarr_clear(arr, bit) \
  do { BITARR_OP(arr, bit, &= ~); } while (0)

/**
 * Toggle a single bit.
 */
#define bitarr_toggle(arr, bit) \
  do { BITARR_OP(arr, bit, ^=); } while (0)

/**
 * Retrieve the value of the specified bit.
 */
#define bitarr_get(arr, bit) \
  (!!BITARR_OP(arr, bit, &))

/**
 * Returns the number of set bits in the bit array.
 *
 * \param arr     Bit array.
 * \param bytelen Byte length of the bit array.
 *
 * \return Number of set bits in the bit array.
 */
int bitarr_count(const T *arr, size_t bytelen);

#undef T

#endif /* DATASTRUCT_BITARR_H */
