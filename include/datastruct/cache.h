/* cache.h -- generic single-block cache */

/**
 * \file cache.h
 *
 * Generic single-block cache.
 *
 * This provides a cache that maintains a single block of memory into which
 * blocks of variable size can be inserted. Items are keyed with a cachekey_t
 * (an unsigned int).
 *
 * Items larger than the remaining capacity in the cache's store will cause
 * older entries to be evicted. The eviction policy is oldest first.
 */

#ifndef DATASTRUCT_CACHE_H
#define DATASTRUCT_CACHE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

#include "base/result.h"

/* ----------------------------------------------------------------------- */

/** An opaque cache handle. */
typedef struct cache cache_t;

/** A structure which holds configuration values. */
typedef struct cacheconfig
{
  int hash_chain_length;   /**< length of hash chains to aim for */
  int nentries_percentage; /**< percentage of cache to allocate for entries */
}
cacheconfig_t;

/** A cache entry key. */
typedef unsigned int cachekey_t;

/* ----------------------------------------------------------------------- */

/**
 * Create a cache.
 *
 * \param[in]  config   Pointer to cache parameters, or NULL for default
 *                      cache parameters.
 * \param[in]  length   Byte length of the cache to allocate.
 * \param[out] cache    Returned pointer to the created cache.
 *
 * \return Error indication.
 */
result_t cache_create(const cacheconfig_t *config,
                      size_t               length,
                      cache_t            **cache);

/**
 * Destroy a cache.
 *
 * \param[in] doomed    Pointer to the cache to destroy.
 */
void cache_destroy(cache_t *doomed);

/**
 * Create a cache in a supplied block of memory.
 *
 * \param[in]  config   Pointer to cache parameters, or NULL for default
 *                      cache parameters.
 * \param[in]  block    Pointer to memory to use.
 * \param[in]  length   Byte length of block.
 * \param[out] cache    Returned pointer to the created cache.
 *
 * \return Error indication.
 */
result_t cache_construct(const cacheconfig_t *config,
                         void                *block,
                         size_t               length,
                         cache_t            **cache);

/**
 * Empty a cache.
 *
 * \param[in] cache     Pointer to the cache to reset.
 */
void cache_empty(cache_t *cache);

/**
 * Find a cached entry.
 *
 * \param[in] cache     Cache handle.
 * \param[in] key       Key.
 *
 * \return Pointer to cached entry, or NULL if not found.
 */
void *cache_get(cache_t *cache, cachekey_t key);

/**
 * Insert an entry into the cache.
 *
 * \param[in]  cache     Cache handle.
 * \param[in]  key       Key.
 * \param[in]  data      Pointer to data to store.
 * \param[in]  length    Length of data.
 * \param[out] inserted  Returned pointer to the inserted data, or NULL if
 *                       not wanted. This valid until the next cache_put
 *                       operation.
 *
 * \return Error indication.
 */
result_t cache_put(cache_t    *cache,
                   cachekey_t  key,
                   void       *data,
                   size_t      length,
                   void      **inserted);

/**
 * Print cache statistics to stdout.
 *
 * \param[in] cache     Cache handle.
 * \param[in] reset     (bool) Reset the cache statistics.
 */
void cache_stats(cache_t *cache, int reset);

/** A structure in which cache info is returned. */
typedef struct cacheinfo
{
  size_t maxlength; /**< Largest storable block length. */
}
cacheinfo_t;

/**
 * Return cache info.
 *
 * \param[in] cache     Cache handle.
 * \param[in] info      Structure to receive the info.
 */
void cache_get_info(const cache_t *cache, cacheinfo_t *info);

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* DATASTRUCT_CACHE_H */
