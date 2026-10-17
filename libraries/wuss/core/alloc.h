/* wuss/core/alloc.h -- wuss - allocation through the configured hooks */

#ifndef WUSS_CORE_ALLOC_H
#define WUSS_CORE_ALLOC_H

#include "types.h"


/* Allocation through a wuss_t's configured hooks (see struct wuss::alloc).
 * Every heap block a wuss_t owns -- windows, icons, icon-pointer arrays, menu
 * nodes -- goes through these so a caller can supply its own allocator. */
static inline void *wuss__malloc(const wuss_t *wuss, size_t size)
{
  return wuss->alloc.malloc(size);
}

static inline void *wuss__realloc(const wuss_t *wuss, void *ptr, size_t size)
{
  return wuss->alloc.realloc(ptr, size);
}

static inline void wuss__free(const wuss_t *wuss, void *ptr)
{
  wuss->alloc.free(ptr);
}

/* Duplicate a NUL-terminated string through an allocator's malloc hook
 * (strdup is POSIX, not C99, so it is not assumed). Returns NULL on OOM or a
 * NULL s. Takes wuss_alloc_t, not wuss_t, so the shared components (which
 * keep a copy of the hooks, not the wuss_t) can use it too. */
static inline char *wuss__alloc_strdup(const wuss_alloc_t *a, const char *s)
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

/* array_grow (utils/array.h) done through an allocator's realloc hook: the
 * next-power-of-two doubling, same element/return semantics (0 ok, 1 OOM).
 * used/need/minimum are element counts. See libraries/utils/array/grow.c. */
static inline int wuss__array_grow(const wuss_alloc_t *a,
                                   void              **block,
                                   size_t              elemsize,
                                   int                 used,
                                   int                *allocated,
                                   int                 need,
                                   int                 minimum)
{
  int   to_allocate;
  void *grown;

  need += used;
  need  = MAX(need, minimum);
  if (need <= *allocated)
    return 0;

  to_allocate = power2gt(need - 1);

  grown = a->realloc(*block, elemsize * (size_t) to_allocate);
  if (grown == NULL)
    return 1;

  *block     = grown;
  *allocated = to_allocate;
  return 0;
}

#endif /* WUSS_CORE_ALLOC_H */
