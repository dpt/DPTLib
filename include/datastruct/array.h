/* array.h -- array utilities */

/**
 * \file array.h
 *
 * Utilities for operating on arrays.
 */

#ifndef DATASTRUCT_ARRAY_H
#define DATASTRUCT_ARRAY_H

#include <stdlib.h>

/* ----------------------------------------------------------------------- */

/* Delete the specified element. */
void array_delete_element(void  *array,
                          size_t elemsize,
                          int    nelems,
                          int    doomed);

/* Delete the specified elements. */
/* last_doomed is inclusive. */
void array_delete_elements(void  *array,
                           size_t elemsize,
                           int    nelems,
                           int    first_doomed,
                           int    last_doomed);

/* ----------------------------------------------------------------------- */

/* Take the contents of an array which used to have elements 'oldwidth' bytes
 * wide and adjust them so they are 'newwidth' bytes wide. Set new bytes to
 * 'wipe_value'. */
void array_stretch1(unsigned char *base,
                    int            nelems,
                    size_t         oldwidth,
                    size_t         newwidth,
                    int            wipe_value);

void array_stretch2(unsigned char *base,
                    int            nelems,
                    size_t         oldwidth,
                    size_t         newwidth,
                    int            wipe_value);

/* Take the contents of an array which used to have elements 'oldwidth' bytes
 * wide and adjust them so they are 'newwidth' bytes wide. */
void array_squeeze1(unsigned char *base,
                    int            nelems,
                    size_t         oldwidth,
                    size_t         newwidth);

void array_squeeze2(unsigned char *base,
                    int            nelems,
                    size_t         oldwidth,
                    size_t         newwidth);

/* Temporary defines until the above functions are renamed. */
#define array_stretch array_stretch2
#define array_squeeze array_squeeze2

/* ----------------------------------------------------------------------- */

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
 * Resize dynamically allocated array to hold exactly the elements it uses.
 *
 * \param block     Pointer to pointer to block. Updated on exit.
 * \param elemsize  Element size.
 * \param used      Number of currently occupied elements.
 * \param allocated Pointer to number of allocated elements. Updated on exit.
 *
 * \return 0 - ok, 1 - out of memory
 */
int array_shrinkwrap(void   **block,
                     size_t   elemsize,
                     int      used,
                     int     *allocated);

/* ----------------------------------------------------------------------- */

#endif /* DATASTRUCT_ARRAY_H */
