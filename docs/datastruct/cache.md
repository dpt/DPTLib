# [DPTLib](https://github.com/dpt/DPTLib) > datastruct > cache

"cache" is a sub-library of DPTLib for managing a generic single-block cache. It provides efficient storage and retrieval of variable-sized data blocks using key-based access with automatic memory management and eviction policies.

The cache maintains a single block of memory into which items of different sizes can be inserted and retrieved. Items are identified by keys (unsigned integers) and the cache automatically handles memory allocation, hash table management, and eviction of older entries when space is needed.

- It supports variable-sized data blocks within a fixed memory footprint.
- It uses an oldest-first eviction policy when capacity is exceeded.
- It provides configurable hash chain lengths and memory allocation ratios.
- Its access patterns should be reasonably efficient for most caching scenarios.

## Cache Architecture

The cache uses a hash table to provide fast key-based lookups while maintaining all cached data within a single contiguous memory block. When new items are added that would exceed the available capacity, older entries are automatically evicted to make space.

The cache can be configured with two key parameters:

- Hash chain length: Controls the trade-off between memory usage and lookup performance
- Entries percentage: Determines how much of the cache memory is allocated for entry metadata vs. actual data storage

## Setup

#### Create a cache:

1. `cache_create()` to allocate and initialise a cache with specified size and configuration.
2. Alternatively, use `cache_construct()` to create a cache within a pre-allocated memory block.

Configuration is optional - pass `NULL` for default parameters:

```C
result_t cache_create(const cacheconfig_t *config,
                      size_t               length,
                      cache_t            **cache);
```

The configuration structure `cacheconfig_t` allows fine-tuning:

```C
typedef struct cacheconfig
{
  int hash_chain_length;   /* length of hash chains to aim for */
  int nentries_percentage; /* percentage of cache to allocate for entries */
}
cacheconfig_t;
```

## Storage and Retrieval

Use `cache_put()` to store data in the cache:

```C
result_t cache_put(cache_t    *cache,
                   cachekey_t  key,
                   void       *data,
                   size_t      length,
                   void      **inserted);
```

It requires a cache handle, a key (unsigned integer), pointer to the data to store, and the data length. It returns a pointer to the inserted data (optional - pass `NULL` if not required). This pointer remains valid until the next `cache_put()` operation.

Use `cache_get()` to retrieve stored data:

```C
void *cache_get(cache_t *cache, cachekey_t key);
```

It requires a cache handle and a key. It returns a pointer to the cached data, or `NULL` if the key is not found.

## Management

Use `cache_empty()` to clear all cached entries:

```C
void cache_empty(cache_t *cache);
```

Use `cache_get_info()` to query cache capabilities:

```C
void cache_get_info(const cache_t *cache, cacheinfo_t *info);
```

It returns information such as the maximum storable block size.

Use `cache_stats()` to print performance statistics:

```C
void cache_stats(cache_t *cache, int reset);
```

Pass a non-zero value for `reset` to clear statistics after printing.

## Cleanup

Use `cache_destroy()` to free resources when finished:

```C
void cache_destroy(cache_t *doomed);
```

Note: Only call this for caches created with `cache_create()`. Caches created with `cache_construct()` in user-supplied memory should not be destroyed this way.

## Limitations

- The cache uses a simple oldest-first eviction policy - more sophisticated policies (LRU, LFU) are not available.
- Keys are limited to unsigned integers - string keys or custom key types are not supported.
- The cache is not thread-safe - external synchronisation is required for concurrent access.
- Cache performance depends heavily on the configured hash chain length and may degrade with poor key distribution.
