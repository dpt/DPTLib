/* shrinkwrap.c */

#include <assert.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/array.h"

int array_shrinkwrap(void   **pblock,
                     size_t   elemsize,
                     int      used,
                     int     *pallocated)
{
  void *block;

  assert(pblock);
  assert(elemsize > 0);
  assert(used > 0);
  assert(pallocated);

  block = realloc(*pblock, elemsize * used);
  if (block == NULL)
    return 1; /* out of memory */

  *pblock     = block;
  *pallocated = used;

  return 0;
}
