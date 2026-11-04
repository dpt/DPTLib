/* utils/alloc/counting.c -- allocator which keeps statistics */

#include <stdlib.h>
#include <string.h>

#include "utils/alloc.h"

/* The one place which must reach the C library allocator, even when
 * alloc-redirect.h was force-included. */
#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup

/* Precedes each block. The union keeps the block after it aligned as malloc
 * would. */
typedef union counted
{
  size_t      size;
  void       *pointer;
  long long   integer;
  long double real;
}
counted_t;

static alloc_stats_t stats;

static void counting_update_range(void)
{
  if (stats.bytes < stats.bytes_low)
    stats.bytes_low = stats.bytes;
  if (stats.bytes > stats.bytes_high)
    stats.bytes_high = stats.bytes;
  if (stats.blocks < stats.blocks_low)
    stats.blocks_low = stats.blocks;
  if (stats.blocks > stats.blocks_high)
    stats.blocks_high = stats.blocks;
}

static void *counting_new_block(counted_t *block, size_t size)
{
  if (block == NULL)
    return NULL;

  block->size   = size;
  stats.bytes  += size;
  stats.blocks += 1;
  stats.allocs += 1;
  counting_update_range();

  return block + 1;
}

void *alloc_counting_malloc(size_t size)
{
  if (size > (size_t) -1 - sizeof(counted_t))
    return NULL;

  return counting_new_block(malloc(sizeof(counted_t) + size), size);
}

void *alloc_counting_calloc(size_t count, size_t size)
{
  size_t total;

  if (size != 0 && count > ((size_t) -1 - sizeof(counted_t)) / size)
    return NULL;

  total = count * size;

  return counting_new_block(calloc(1, sizeof(counted_t) + total), total);
}

void *alloc_counting_realloc(void *ptr, size_t size)
{
  counted_t *block;
  size_t     old_size;

  if (ptr == NULL)
    return alloc_counting_malloc(size);

  if (size > (size_t) -1 - sizeof(*block))
    return NULL;

  block    = (counted_t *) ptr - 1;
  old_size = block->size;

  block = realloc(block, sizeof(*block) + size);
  if (block == NULL)
    return NULL;

  block->size  = size;
  stats.bytes += size - old_size;
  counting_update_range();

  return block + 1;
}

void alloc_counting_free(void *ptr)
{
  counted_t *block;

  if (ptr == NULL)
    return;

  block         = (counted_t *) ptr - 1;
  stats.bytes  -= block->size;
  stats.blocks -= 1;
  stats.frees  += 1;
  counting_update_range();

  free(block);
}

char *alloc_counting_strdup(const char *s)
{
  size_t len;
  char  *copy;

  len  = strlen(s) + 1;
  copy = alloc_counting_malloc(len);
  if (copy != NULL)
    memcpy(copy, s, len);

  return copy;
}

void alloc_counting_get_stats(alloc_stats_t *out)
{
  *out = stats;
}

void alloc_counting_reset_lows(void)
{
  stats.bytes_low  = stats.bytes;
  stats.blocks_low = stats.blocks;
}
