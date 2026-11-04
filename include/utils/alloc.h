/* utils/alloc.h -- pluggable allocator hooks */

/**
 * \file alloc.h
 *
 * Allocator hooks, for libraries which let their caller supply the allocator
 * behind every heap block they own.
 */

#ifndef UTILS_ALLOC_H
#define UTILS_ALLOC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

/**
 * Allocator hooks. \c malloc, \c realloc and \c free must behave like their
 * C library namesakes: same argument and return conventions, realloc(NULL,
 * n) == malloc(n) and free(NULL) a no-op.
 */
typedef struct alloc
{
  void *(*malloc)(size_t size);
  void *(*realloc)(void *ptr, size_t size);
  void  (*free)(void *ptr);
}
alloc_t;

/** The default allocator: the C library malloc, realloc and free. */
extern const alloc_t alloc_default;

/**
 * An allocator which keeps count of the bytes it has live. It sits on the C
 * library allocator and adds a small header to each block, so only its own
 * hooks can free what it allocates. The count is shared by all users.
 */
extern const alloc_t alloc_counting;

/**
 * Report how many bytes are allocated through \ref alloc_counting.
 *
 * \return Bytes requested and not yet freed, not counting block headers.
 */
size_t alloc_counting_bytes(void);

/**
 * Duplicate a NUL-terminated string through an allocator's malloc hook.
 *
 * strdup is POSIX, not C99, so it is not assumed.
 *
 * \param[in] a Allocator.
 * \param[in] s String to duplicate, or NULL.
 *
 * \return Copy of s, or NULL if s is NULL or out of memory.
 */
char *alloc_strdup(const alloc_t *a, const char *s);

#ifdef __cplusplus
}
#endif

#endif /* UTILS_ALLOC_H */
