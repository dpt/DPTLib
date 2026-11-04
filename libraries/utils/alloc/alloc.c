/* utils/alloc/alloc.c -- pluggable allocator hooks */

#include <stdlib.h>
#include <string.h>

#include "utils/alloc.h"

const alloc_t alloc_default =
{
  malloc, realloc, free
};

/* ----------------------------------------------------------------------- */

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

static size_t counted_bytes;

static void *counting_malloc(size_t size)
{
  counted_t *block;

  if (size > (size_t) -1 - sizeof(*block))
    return NULL;

  block = malloc(sizeof(*block) + size);
  if (block == NULL)
    return NULL;

  block->size    = size;
  counted_bytes += size;

  return block + 1;
}

static void *counting_realloc(void *ptr, size_t size)
{
  counted_t *block;
  size_t     old_size;

  if (ptr == NULL)
    return counting_malloc(size);

  if (size > (size_t) -1 - sizeof(*block))
    return NULL;

  block    = (counted_t *) ptr - 1;
  old_size = block->size;

  block = realloc(block, sizeof(*block) + size);
  if (block == NULL)
    return NULL;

  block->size    = size;
  counted_bytes += size - old_size;

  return block + 1;
}

static void counting_free(void *ptr)
{
  counted_t *block;

  if (ptr == NULL)
    return;

  block          = (counted_t *) ptr - 1;
  counted_bytes -= block->size;

  free(block);
}

const alloc_t alloc_counting =
{
  counting_malloc, counting_realloc, counting_free
};

size_t alloc_counting_bytes(void)
{
  return counted_bytes;
}

/* ----------------------------------------------------------------------- */

char *alloc_strdup(const alloc_t *a, const char *s)
{
  size_t len;
  char  *copy;

  if (s == NULL)
    return NULL;

  len  = strlen(s) + 1;
  copy = a->malloc(len);
  if (copy != NULL)
    memcpy(copy, s, len);

  return copy;
}
