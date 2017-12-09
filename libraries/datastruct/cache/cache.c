/* cache.c -- generic single block cache */

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/barith.h"

#include "datastruct/cache.h"

/* ----------------------------------------------------------------------- */

/* Organisation
 * ------------
 * The cache is a single block of memory laid out like this:
 *
 * +--------+------+---------+-------------------------------------------+
 * | Header | Bins | Entries | Store                                     |
 * +--------+------+---------+-------------------------------------------+
 *
 * - Header is a cache_t.
 * - Bins is an array of pointers to linked lists of entries, all of which
 *   hash to that bin.
 *   The last + 1 hash bin is the complement of the earlier lists: a linked
 *   list of free entries (all the entries not linked from elsewhere).
 * - Entries are the items stored in the cache.
 * - Store is the actual stored data.
 *
 * Although entries and stored data could live as a single allocation within
 * the store I laid the cache out this way to get as much similar data as
 * possible into the store (to facilitate cache data sharing). The impact of
 * this is that there is a fixed pool of entries, which you can run out of,
 * just like you can run out of store space.
 *
 *
 * TODO
 * ----
 * On 64-bit targets the pointers in entry and free structures will gobble up
 * quite a bit of space. Given that the caches aren't likely to exceed 2^32
 * bytes (and the entries within will be aligned to a 8-byte boundary) we
 * could use 32-bit offsets at the cost of complicating the code. The length
 * values could similarly be reduced in line with the size of the cache.
 * (e.g. A 256K cache could survive using only 16-bit values for offsets,
 * instead of pointers, and lengths).
 *
 * cache_create and cache_construct are very similar - choose just one to
 * support.
 *
 *
 * References
 * ----------
 * See The Art of Computer Programming Vol.1 3rd ed. p440 "Liberation with
 * sorted list".
 *
 */

/* ----------------------------------------------------------------------- */

/* Define to enable cache debugging and extended stats. */
/* #define CACHE_DEBUG */

/* Set to 1 to output slower, more advanced stats. */
#define ADVANCED_STATS 1

/* ----------------------------------------------------------------------- */

/* A type representing time within the cache. */
typedef unsigned int cachetime_t;

/* The cache itself. */
struct cache
{
  cachetime_t         time;         /* a monotonic timer which is incremented
                                       on each operation */
  int                 nbins;        /* the number of hash bins */
  struct cacheentry **bins;         /* pointer to array of linked list start
                                       entries (the final entry [nbins+1] is
                                       the free entry chain) */
  int                 nentries;     /* the number of entries */
  struct cacheentry  *entries;      /* pointer to array of entries */
  struct free        *firstfree;    /* first free block within the cache */
  size_t              storelength;  /* length (bytes) of storage memory
                                       block, excluding overheads */
  unsigned char      *store;        /* pointer to cache memory block */
  struct
  {
    int               hits;         /* number of cache hits */
    int               misses;       /* number of cache misses */
    int               evictions;    /* number of evictions */
  }
  stats;
#ifdef CACHE_DEBUG
  struct
  {
    int               storeused;    /* bytes used in store */
    int               usedentries;  /* number of used entries */
  }
  debug;
#endif
};

/* An entry stored within the cache. */
/* Note: It should be 'struct entry' but that clashes with a definition in
 * <search.h>. */
typedef struct cacheentry
{
  struct cacheentry *next;          /* linked list next pointer */
  cachekey_t         key;           /* the key to identify the entry */
  void              *data;          /* pointer to data in the store */
  size_t             length;        /* length in bytes of stored data */
  cachetime_t        time;          /* time of last use */
}
entry_t;

/* A free block within the store. */
typedef struct free
{
  struct free       *next;          /* linked list next pointer */
  size_t             length;        /* length in bytes of the free block */
}
free_t;

/* ----------------------------------------------------------------------- */

/* default configuration parameters */
static const cacheconfig_t default_config =
{
  .hash_chain_length   = 8,
  .nentries_percentage = 20
};

/* ----------------------------------------------------------------------- */

/* debug mode consistency check: walks all entries reachable from the bins to
 * ensure they're all accounted for */
#ifdef CACHE_DEBUG
static void cache_check(cache_t *c, int nextra)
{
  int      nentries;
  int      i;
  entry_t *e;

  assert(c->debug.storeused >= 0);
  assert(c->debug.storeused <= c->storelength);
  assert(c->debug.usedentries >= 0);
  assert(c->debug.usedentries <= c->nentries);

  /* walk all entries AND the free list chain */
  nentries = 0;
  for (i = 0; i < c->nbins + 1; i++)
    for (e = c->bins[i]; e != NULL; e = e->next)
      nentries++;

  assert(nentries == c->nentries + nextra);
}
#else
#define cache_check(c, nextra)
#endif

/* reset the stats */
static void cache_reset_stats(cache_t *c)
{
  c->stats.hits      = 0;
  c->stats.misses    = 0;
  c->stats.evictions = 0;
}

result_t cache_create(const cacheconfig_t *config,
                      size_t               length,
                      cache_t            **new_cache)
{
  const size_t quantum = sizeof(free_t);

  int      nentries;
  int      nbins;
  size_t   szheader;
  size_t   ofbins;
  size_t   szbins;
  size_t   ofentries;
  size_t   szentries;
  size_t   ofstore;
  size_t   szstore;
  size_t   szcache;
  cache_t *c;

  /* validate arguments */
  if (config == NULL)
    config = &default_config; /* use default configuration if none specified */
  if (length < 512)
    return result_BAD_ARG;
  if (new_cache == NULL)
    return result_NULL_ARG;

  /* validate the configuration values */
  if (config->hash_chain_length < 1)
    return result_BAD_ARG;
  /* 5..95% is the useful range, at a guess, for nentries */
  if (config->nentries_percentage < 5 || config->nentries_percentage > 95)
    return result_BAD_ARG;

  *new_cache = NULL;

  /* compute the number of entries to allocate based on the size of the cache
   * (and the user's configured value) */
  nentries = (int)(length / sizeof(*c->entries) *
                   config->nentries_percentage / 100);
  if (nentries < 1)
    return result_BAD_ARG;

  /* calculate the number of hash bins using the number of entries and the
   * user's desired average hash chain length. the result must be a power of
   * two to match the hash function. */
  nbins = power2le(nentries / config->hash_chain_length);

  /* work out the block layout */
  szheader  = sizeof(*c);
  ofbins    = szheader;
  szbins    = sizeof(*c->bins) * (nbins + 1);
  ofentries = szheader + szbins;
  szentries = sizeof(*c->entries) * nentries;
  ofstore   = szheader + szbins + szentries;
  /* align store to a free_t-friendly alignment */
  ofstore   = ((ofstore + quantum - 1) / quantum) * quantum;
  szstore   = length - ofstore;
  /* round store to a multiple of free_t (otherwise risk putting a 8-byte
   * free_t into a 4-byte spare block) */
  szstore   = (szstore / quantum) * quantum;
  szcache   = ofstore + szstore;

  if (szcache > length)
    return result_BAD_ARG; /* calculations didn't work */

  /* allocate the cache as a single block */
  c = malloc(szcache);
  if (c == NULL)
    return result_OOM;

  /* work out structure locations */
  c->bins    = (struct cacheentry **)((char *) c + ofbins);
  c->entries =  (struct cacheentry *)((char *) c + ofentries);
  c->store   =               (unsigned char *) c + ofstore;

  c->nbins    = nbins;
  c->nentries = nentries;

  c->storelength = szstore;

  cache_empty(c);

  *new_cache = c;

  return result_OK;
}

void cache_destroy(cache_t *doomed)
{
  free(doomed);
}

result_t cache_construct(const cacheconfig_t *config,
                         void                *block,
                         size_t               length,
                         cache_t            **new_cache)
{
  const size_t quantum = sizeof(free_t);

  int      nentries;
  int      nbins;
  size_t   szheader;
  size_t   ofbins;
  size_t   szbins;
  size_t   ofentries;
  size_t   szentries;
  size_t   ofstore;
  size_t   szstore;
  size_t   szcache;
  cache_t *c;

  /* validate arguments */
  if (config == NULL)
    config = &default_config; /* use default configuration if none specified */
  if (block == NULL || new_cache == NULL)
    return result_NULL_ARG;
  if (length < 512)
    return result_BAD_ARG;

  /* validate the configuration values */
  if (config->hash_chain_length < 1)
    return result_BAD_ARG;
  /* 5..95% is the useful range, at a guess, for nentries */
  if (config->nentries_percentage < 5 || config->nentries_percentage > 95)
    return result_BAD_ARG;

  *new_cache = NULL;

  /* compute the number of entries to allocate based on the size of the cache
   * (and the user's configured value) */
  nentries = (int)(length / sizeof(*c->entries) *
                   config->nentries_percentage / 100);
  if (nentries < 1)
    return result_BAD_ARG;

  /* calculate the number of hash bins using the number of entries and the
   * user's desired average hash chain length. the result must be a power of
   * two to match the hash function. */
  nbins = power2le(nentries / config->hash_chain_length);

  /* work out the block layout */
  szheader  = sizeof(*c);
  ofbins    = szheader;
  szbins    = sizeof(*c->bins) * (nbins + 1);
  ofentries = szheader + szbins;
  szentries = sizeof(*c->entries) * nentries;
  ofstore   = szheader + szbins + szentries;
  /* align store to a free_t-friendly alignment */
  ofstore   = ((ofstore + quantum - 1) / quantum) * quantum;
  szstore   = length - ofstore;
  /* round store to a multiple of free_t (otherwise risk putting a 8-byte
   * free_t into a 4-byte spare block) */
  szstore   = (szstore / quantum) * quantum;
  szcache   = szheader + szbins + szentries + szstore;

  if (szcache > length)
    return result_BAD_ARG; /* calculations didn't work */

  c = block;

  /* work out structure locations */
  c->bins    = (struct cacheentry **)((char *) c + ofbins);
  c->entries = (struct cacheentry *)((char *) c + ofentries);
  c->store   = (unsigned char *) c + ofstore;

  c->nbins    = nbins;
  c->nentries = nentries;

  c->storelength = szstore;

  cache_empty(c);

  *new_cache = c;

  return result_OK;
}

/* reset the cache */
void cache_empty(cache_t *c)
{
  int i;

  assert(c);
  assert(c->nbins    > 0 && c->nbins    < 100000); /* sanity check */
  assert(c->nentries > 0 && c->nentries < 100000); /* sanity check */

  c->time = 0;

  /* empty all of the hash bins */
  for (i = 0; i < c->nbins; i++)
    c->bins[i] = NULL;

  /* chain all of the entries together */
  for (i = 0; i < c->nentries - 1; i++)
  {
    c->entries[i].next = &c->entries[i + 1];
    c->entries[i].data = NULL;
  }

  /* terminate the final entry with a NULL */
  c->entries[i].next = NULL;
  c->entries[i].data = NULL;

  /* point the final+1 hash bin to the chain */
  c->bins[c->nbins] = &c->entries[0];

  /* unused regions in the store become free list entries */
  c->firstfree = (free_t *) &c->store[0];

  /* initialise the as-yet unoccupied store as a single free_t */
  c->firstfree->next   = NULL;
  c->firstfree->length = c->storelength;

#ifdef CACHE_DEBUG
  /* reset debugging stats */
  c->debug.storeused   = 0;
  c->debug.usedentries = 0;
#endif

  cache_check(c, 0);

  /* reset statistics */
  cache_reset_stats(c);
}

/* 0x9e3779b9 is the golden ratio in fixed point 1.31 format */
#define HASH(i, nbins, key) \
  do                        \
  {                         \
    i = key;                \
    i ^= i >> 16;           \
    i ^= i >> 8;            \
    i *= 0x9e3779b9;        \
    i &= nbins - 1;         \
  }                         \
  while (0)

void *cache_get(cache_t *c, cachekey_t key)
{
  int      i;
  entry_t *e;

  assert(c);

  if (c == NULL)
    return NULL; /* no error return available here */

  HASH(i, c->nbins, key);

  for (e = c->bins[i]; e != NULL; e = e->next)
    if (e->key == key)
    {
      e->time = c->time++;
      c->stats.hits++;
      return e->data;
    }

  c->stats.misses++;
  return NULL;
}

/* unlink the specified entry and remove its associated store block */
static void purge(cache_t *c, entry_t *purgeent, int entbin)
{
  assert(c);
  assert(purgeent);
  assert(entbin >= 0 && entbin < c->nbins);

  /* unlink the entry from its chain */
  if (c->bins[entbin] == purgeent)
  {
    /* entry is at start of chain */

    c->bins[entbin] = purgeent->next; /* unlink */
  }
  else
  {
    /* entry is later in the chain */

    entry_t *e;

    /* find the entry that points to the entry to be removed */
    for (e = c->bins[entbin]; e->next != purgeent; e = e->next)
      ;

    e->next = purgeent->next; /* unlink */
  }

  /* entry is now unlinked from its hash chain: link it into the free entries
   * chain */
  purgeent->next = c->bins[c->nbins];
  c->bins[c->nbins] = purgeent;

  cache_check(c, 0);

  /* free the store block */
  {
    free_t *free;      /* area to free */
    size_t  length;    /* its size */
    size_t  newlength; /* its size (gets adjusted) */
    free_t *left, *right;

    assert(purgeent->length >= sizeof(*free));

    free      = (free_t *) purgeent->data;
    length    = purgeent->length;
    newlength = length;

    if (c->firstfree == NULL)
    {
      /* if the cache is full then there's no free list */

      free->next   = NULL;
      free->length = newlength;

      c->firstfree = free;
    }
    else
    {
      /* move 'left' and 'right' until they straddle the free location */
      for (left = (free_t *) &c->firstfree; (right = left->next) != NULL; left = right)
        if (right > free)
          break;

      /* check upper bound */
      if (right != NULL && (char *) free + newlength == (char *) right)
      {
        /* 'right' is a subsequent adjacent free block: absorb it */
        newlength += right->length;
        free->next = right->next;
      }
      else
      {
        /* subsequent allocated block, or nothing */
        free->next = right;
      }

      /* check lower bound */
      if (left != (free_t *) &c->firstfree &&
          (char *) left + left->length == (char *) free)
      {
        /* 'left' is a preceding adjacent free block: merge with it */
        left->length += newlength;
        left->next    = free->next;
      }
      else
      {
        /* preceding allocated block, or nothing */
        left->next   = free;
        free->length = newlength;
      }

      /* firstfree points to the lowest block */
      if (free < c->firstfree)
        c->firstfree = free;
    }

    cache_check(c, 0);

#ifdef CACHE_DEBUG
    c->debug.storeused -= length;
    c->debug.usedentries--;
#endif
  }

  cache_check(c, 0);
}

/* evict an entry */
static entry_t *evict(cache_t *c)
{
  entry_t     *evictee;
  int          evicteebin;
  unsigned int oldest;
  int          i;

  assert(c);

  evictee    = NULL;
  evicteebin = -1;
  oldest     = UINT_MAX;

  for (i = 0; i < c->nbins; i++)
  {
    entry_t *e;

    e = c->bins[i];
    if (e != NULL && e->time < oldest)
    {
      evictee    = e;
      evicteebin = i;
      oldest     = e->time;
    }
  }

  assert(evictee    != NULL);
  assert(evicteebin != -1);

  purge(c, evictee, evicteebin);

  c->stats.evictions++;

  return evictee;
}

result_t cache_put(cache_t    *c,
                   cachekey_t  key,
                   void       *data,
                   size_t      length,
                   void      **inserted)
{
  const size_t quantum = sizeof(free_t);

  size_t  rounded_length;
  void   *storeptr;

  assert(c    != NULL);
  assert(data != NULL);
  assert(length > 0);

  if (c == NULL || data == NULL)
    return result_NULL_ARG;
  if (length == 0)
    return result_BAD_ARG;

  if (inserted)
    *inserted = NULL;

  /* round up the length to a multiple of free_t's length so that any gaps
   * between blocks are no smaller than free_t. */
  rounded_length = ((length + quantum - 1) / quantum) * quantum;
  assert(rounded_length > 0);

  if (rounded_length > c->storelength)
    return result_TOO_BIG;

  cache_check(c, 0);

  {
    free_t *left, *right;

    /* find a free block */

    /* If we can't find one of at least the size we need then evict and
     * retry. This isn't great but ensures that we will eventually get a
     * block of at least the right size. */
    for (;;)
    {
      for (left = (free_t *) &c->firstfree; (right = left->next) != NULL; left = right)
        if (right->length >= rounded_length)
          break;

      if (right)
        break;

      (void) evict(c);
    }

    /* here free block 'right' is big enough */

    assert(right->length > 0);
    assert(right->length >= rounded_length);
    assert(right->length <= c->storelength);

    if (right->length > rounded_length)
    {
      /* the free block is bigger than required: use the end of it */
      right->length -= rounded_length;
      storeptr = (char *) right + right->length;
    }
    else
    {
      /* the free block was exactly the right size: use it all */
      left->next = right->next;
      storeptr = right;
    }

    memcpy(storeptr, data, length);

#ifdef CACHE_DEBUG
    c->debug.storeused += rounded_length;
#endif
  }

  cache_check(c, 0);

  /* the block has been inserted into the store. but there's not necessarily
   * a free entry with which to reference it. */

  {
    entry_t *entry;
    int      i;

    if (c->bins[c->nbins] == NULL)
    {
      /* the free list is empty */

      entry = evict(c);

      /* the only freelist entry should be the freed entry after eviction */
      assert(c->bins[c->nbins] == entry);
      assert(entry->next == NULL);

      cache_check(c, 0);
    }
    else
    {
      entry = c->bins[c->nbins];
    }

    /* unlink */
    c->bins[c->nbins] = entry->next;

    cache_check(c, -1);

    /* append entry to the end of the chain */

    HASH(i, c->nbins, key);

    if (c->bins[i] == NULL)
    {
      /* an empty hash bin */
      c->bins[i] = entry;
    }
    else
    {
      /* insert at the end of the chain (so that we find older entries sooner
       * when evicting) */
      entry_t *entry2;

      for (entry2 = c->bins[i]; entry2->next != NULL; entry2 = entry2->next)
        ;
      entry2->next = entry;
    }

    entry->next   = NULL;
    entry->key    = key;
    entry->data   = storeptr;
    entry->length = rounded_length;
    entry->time   = c->time++;

#ifdef CACHE_DEBUG
    c->debug.usedentries++;
#endif
  }

  cache_check(c, 0);

  if (inserted)
    *inserted = storeptr;

  return result_OK;
}

void cache_stats(cache_t *c, int reset)
{
  assert(c     != NULL);
  assert(reset >= 0 && reset <= 1);

  if (c == NULL)
    return;

#ifdef CACHE_DEBUG
  int      nbinsused;
  int      i;
  int      nentriesused;
  entry_t *entry;
  int      nfreeentries;
  double   mean;

  cache_check(c, 0);

  /* count the number of hash bins used */
  nbinsused = 0;
  for (i = 0; i < c->nbins; i++)
    if (c->bins[i])
      nbinsused++;

  /* count the number of entries used */
  nentriesused = 0;
  for (i = 0; i < c->nbins; i++)
    for (entry = c->bins[i]; entry != NULL; entry = entry->next)
      nentriesused++;

  assert(nentriesused == c->debug.usedentries);

  /* count the number of free entries */
  nfreeentries = 0;
  for (entry = c->bins[c->nbins]; entry != NULL; entry = entry->next)
    nfreeentries++;

  assert(nfreeentries == c->nentries - c->debug.usedentries);

  mean = (double) c->debug.usedentries / c->nbins;

  printf("cache stats at time %d:\n"
         "store used         = %d of %zu bytes (%zu%%)\n"
         "entries used       = %d of %d (%d%%) [%zu of %zu bytes @ %zu bytes each]\n"
         "hash bins used     = %d of %d (%d%%) [%zu of %zu bytes @ %zu bytes each]\n"
         "average hash chain = %.2f long\n"
         "average entry size = %.2f bytes\n",

         c->time,

         c->debug.storeused,
         c->storelength,
         c->debug.storeused * 100 / c->storelength,

         c->debug.usedentries,
         c->nentries,
         c->debug.usedentries * 100 / c->nentries,
         c->debug.usedentries * sizeof(*c->entries),
         c->nentries * sizeof(*c->entries),
         sizeof(*c->entries),

         nbinsused,
         c->nbins,
         nbinsused * 100 / c->nbins,
         nbinsused * sizeof(*c->bins),
         c->nbins * sizeof(*c->bins),
         sizeof(*c->bins),

         mean,

         (c->debug.usedentries > 0) ? (double) c->debug.storeused / c->debug.usedentries : 0.0);

  if (ADVANCED_STATS)
  {
    double variance;

    printf("mean chain length  = %.2f\n", mean);

    variance = 0;
    for (i = 0; i < c->nbins; i++)
    {
      int    len;
      double delta;

      len = 0;
      for (entry = c->bins[i]; entry != NULL; entry = entry->next)
        len++;

      delta = len - mean;
      variance += delta * delta;
    }

    variance = variance / c->nbins;

    printf("variance           = %.2f\n", variance);
    printf("std. dev           = %.2f\n", sqrt(variance));
  }
#endif

  printf("hits               = %d\n"
         "misses             = %d\n"
         "evictions          = %d\n",
         c->stats.hits,
         c->stats.misses,
         c->stats.evictions);

  if (reset)
    cache_reset_stats(c);
}

void cache_get_info(const cache_t *c, cacheinfo_t *info)
{
  assert(c    != NULL);
  assert(info != NULL);

  if (c == NULL || info == NULL)
    return;

  info->maxlength = c->storelength;
}
