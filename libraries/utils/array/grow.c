/* utils/array/grow.c -- grow an array */

#include <assert.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "utils/alloc.h"
#include "utils/array.h"
#include "utils/barith.h"

/* a function, not the bare name, so Fortify's realloc macro applies */
static void *grow_realloc(void *ptr, size_t size)
{
  return realloc(ptr, size);
}

/* used, need, minimum - specified as number of elements (not bytes) */
static int grow(void  *(*reallocfn)(void *, size_t),
                void **pblock,
                size_t elemsize,
                int    used,
                int   *pallocated,
                int    need,
                int    minimum)
{
  int   to_allocate;
  void *block;

  assert(pblock);
  assert(elemsize > 0);
  assert(used >= 0);
  assert(pallocated);
  assert(need > 0);
  assert(minimum > 0);

  need += used;
  need  = MAX(need, minimum);

  if (need <= *pallocated)
    return 0; /* block has enough spare space */

  /* Rounding up to the next largest power of two strategy. */
  to_allocate = power2gt(need - 1); /* subtract 1 to make greater or equal */

  block = reallocfn(*pblock, elemsize * to_allocate);
  if (block == NULL)
    return 1; /* out of memory */

  *pblock     = block;
  *pallocated = to_allocate;

  return 0;
}

int array_grow(void **pblock,
               size_t elemsize,
               int    used,
               int   *pallocated,
               int    need,
               int    minimum)
{
  return grow(grow_realloc, pblock, elemsize, used, pallocated, need,
              minimum);
}

int array_grow_alloc(const alloc_t *a,
                     void         **pblock,
                     size_t         elemsize,
                     int            used,
                     int           *pallocated,
                     int            need,
                     int            minimum)
{
  assert(a);

  return grow(a->realloc, pblock, elemsize, used, pallocated, need,
              minimum);
}
