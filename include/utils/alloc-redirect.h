/* utils/alloc-redirect.h -- send C library allocation to alloc_counting_* */

/**
 * \file alloc-redirect.h
 *
 * Force-included into every file of DPTLib and its dependants when
 * DPTLIB_ALLOC_COUNT is set. Do not include it by hand.
 *
 * Blocks are only interchangeable with others from these functions. Memory
 * which a C library or other uninstrumented code allocates must not be given
 * to the redirected \c free, nor the reverse.
 */

#ifndef UTILS_ALLOC_REDIRECT_H
#define UTILS_ALLOC_REDIRECT_H

/* These declare the real functions, so must come before the macros. */
#include <stdlib.h>
#include <string.h>

/* Object-like, so that functions used as values are redirected too. */
#define malloc  alloc_counting_malloc
#define calloc  alloc_counting_calloc
#define realloc alloc_counting_realloc
#define free    alloc_counting_free
#define strdup  alloc_counting_strdup

/* After the macros, so that the alloc_t member names match all its users. */
#include "alloc.h"

#endif /* UTILS_ALLOC_REDIRECT_H */
