/* bsearch.h -- binary searching arrays */

#ifndef UTILS_BSEARCH_H
#define UTILS_BSEARCH_H

#include <stddef.h>

/* Binary search an array 'nelems' long for 'want'.
 * Each array element is 'stride' bytes wide. */

/* Example:
 *
 * To search an array, ordered by key, composed of the following structures:
 *
 * struct foo
 * {
 *   int key;
 *   int value;
 * }
 * foo_t;
 *
 * foo_t foos[100];
 *
 * index = bsearch_int(&foos[0].key, 100, sizeof(foo_t), 42);
 *
 * 'index' is the index of the element, or < 0 if not found.
 */

int bsearch_short(const short *array,
                  unsigned int nelems,
                  size_t       stride,
                  short        want);

int bsearch_ushort(const unsigned short *array,
                   unsigned int          nelems,
                   size_t                stride,
                   unsigned short        want);

int bsearch_int(const int   *array,
                unsigned int nelems,
                size_t       stride,
                int          want);

int bsearch_uint(const unsigned int *array,
                 unsigned int        nelems,
                 size_t              stride,
                 unsigned int        want);

#endif /* UTILS_BSEARCH_H */
