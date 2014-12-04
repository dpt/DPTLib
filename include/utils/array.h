/* --------------------------------------------------------------------------
 *    Name: array.h
 * Purpose: Array utilities
 * ----------------------------------------------------------------------- */

/**
 * \file array.h
 *
 * Utilities for operating on arrays.
 */

#ifndef UTILS_ARRAY_H
#define UTILS_ARRAY_H

#include <stdlib.h>

/**
 * Returns the number of elements in the specified array.
 */
#define NELEMS(a) ((int) (sizeof(a) / sizeof((a)[0])))

/**
 * Grow a dynamically allocated array as required by doubling.
 *
 * 'block' can be NULL to perform an initial alloc.
 * Start with used == allocated == 0.
 *
 * \param block     Pointer to pointer to block. Updated on exit.
 * \param elemsize  Element size.
 * \param used      Number of currently occupied elements.
 * \param allocated Pointer to number of allocated elements. Updated on exit.
 * \param need      Number of unoccupied elements we need.
 * \param minimum   Minimum number of elements to allocate.
 *
 * \return 0 - ok, 1 - out of memory
 */
int array_grow(void   **block,
               size_t   elemsize,
               int      used,
               int     *allocated,
               int      need,
               int      minimum);

/**
 * Shrink a dynamically allocated array to have no free entries.
 *
 * \param block     Pointer to pointer to block. Updated on exit.
 * \param elemsize  Element size.
 * \param used      Number of currently occupied elements.
 * \param allocated Pointer to number of allocated elements. Updated on exit.
 *
 * \return 0 - ok, 1 - out of memory
 */
int array_shrink(void   **block,
                 size_t   elemsize,
                 int      used,
                 int     *allocated);

#endif /* UTILS_ARRAY_H */
