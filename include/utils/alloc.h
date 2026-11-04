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
 * Statistics for the alloc_counting_* functions. Bytes are as requested, not
 * counting block headers or C library overhead.
 */
typedef struct alloc_stats
{
  size_t bytes;       /**< Bytes allocated now. */
  size_t bytes_low;   /**< Lowest \c bytes since the last reset. */
  size_t bytes_high;  /**< Highest \c bytes ever. */
  size_t blocks;      /**< Blocks allocated now. */
  size_t blocks_low;  /**< Lowest \c blocks since the last reset. */
  size_t blocks_high; /**< Highest \c blocks ever. */
  size_t allocs;      /**< Blocks ever allocated. */
  size_t frees;       /**< Blocks ever freed. */
}
alloc_stats_t;

/**
 * Counterparts of the C library allocator which keep statistics. They add a
 * small header to each block, so a block from one must only be released or
 * resized by another. Statistics are shared by all callers and are not
 * thread-safe.
 *
 * Setting the DPTLIB_ALLOC_COUNT build option sends all of DPTLib's and its
 * dependants' malloc, calloc, realloc, free and strdup calls here, and so
 * alloc_default too.
 */
void *alloc_counting_malloc(size_t size);
void *alloc_counting_calloc(size_t count, size_t size);
void *alloc_counting_realloc(void *ptr, size_t size);
void  alloc_counting_free(void *ptr);
char *alloc_counting_strdup(const char *s);

/**
 * Retrieve the allocation statistics.
 *
 * \param[out] out Filled in with the statistics.
 */
void alloc_counting_get_stats(alloc_stats_t *out);

/**
 * Restart the lowest figures from the current ones. They otherwise read
 * zero, as nothing is allocated when a program starts.
 */
void alloc_counting_reset_lows(void);

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
