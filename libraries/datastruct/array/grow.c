/* grow.c */

#include <assert.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/array.h"

int array_grow(void   **pblock,
               size_t   elemsize,
               int      used,
               int     *pallocated,
               int      need,
               int      minimum)
{
  int   allocated;
  void *block;

  assert(pblock);
  assert(elemsize > 0);
  assert(used >= 0);
  assert(pallocated);
  assert(need > 0);
  assert(minimum > 0);

  allocated = *pallocated;

  if (used + need <= allocated)
    return 0; /* block has enough spare space */

  allocated *= 2; /* doubling block size strategy */
  if (allocated < need)
    allocated = need;
  if (allocated < minimum)
    allocated = minimum;

  block = realloc(*pblock, elemsize * allocated);
  if (block == NULL)
    return 1; /* out of memory */

  *pblock     = block;
  *pallocated = allocated;

  return 0;
}
