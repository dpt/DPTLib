/* bsearch.h -- binary searching arrays */

#ifndef UTILS_BSEARCH_H
#define UTILS_BSEARCH_H

#include <stddef.h>

/* Binary search an array 'nelems' long for 'want'.
 * Each array element is 'stride' bytes wide.
 */

int bsearch_short(const short *array,
                  size_t       nelems,
                  size_t       stride,
                  short        want);

int bsearch_ushort(const unsigned short *array,
                   size_t                nelems,
                   size_t                stride,
                   unsigned short        want);

int bsearch_int(const int *array,
                size_t     nelems,
                size_t     stride,
                int        want);

int bsearch_uint(const unsigned int *array,
                 size_t              nelems,
                 size_t              stride,
                 unsigned int        want);

#endif /* UTILS_BSEARCH_H */
