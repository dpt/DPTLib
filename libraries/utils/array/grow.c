/* grow.c */

#include <assert.h>
#include <stdlib.h>

#include "utils/array.h"

/* used, need, minimum - specified as number of elements (not bytes) */
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

  need += used;

  if (need <= allocated)
    return 0; /* block has enough spare space */

  if (allocated < minimum)
    allocated = minimum;

  while (allocated < need)
    allocated *= 2; /* doubling block size strategy */

  block = realloc(*pblock, elemsize * allocated);
  if (block == NULL)
    return 1; /* out of memory */

  *pblock     = block;
  *pallocated = allocated;

  return 0;
}
