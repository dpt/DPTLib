/* grow.c -- grow an array */

#include <assert.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "utils/array.h"
#include "utils/barith.h"

/* used, need, minimum - specified as number of elements (not bytes) */
int array_grow(void   **pblock,
               size_t   elemsize,
               int      used,
               int     *pallocated,
               int      need,
               int      minimum)
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

  if (need < minimum)
    need = minimum;

  if (need <= *pallocated)
    return 0; /* block has enough spare space */

  /* Rounding up to the next largest power of two strategy. */
  to_allocate = power2gt(need - 1); /* subtract 1 to make greater or equal */

  block = realloc(*pblock, elemsize * to_allocate);
  if (block == NULL)
    return 1; /* out of memory */

  *pblock     = block;
  *pallocated = to_allocate;

  return 0;
}
