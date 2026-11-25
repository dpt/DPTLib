/* datastruct/atom/destroy.c -- atoms */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/atom.h"

#include "impl.h"

void atom_destroy(atom_set_t *s)
{
  unsigned int i;

  if (s == NULL)
    return;

  /* delete all location pools */

  for (i = 0; i < s->l_used; i++)
    s->alloc->free(s->locpools[i].locs);

  /* delete the location pool pointers */

  s->alloc->free(s->locpools);

  /* delete all block pools */

  for (i = 0; i < s->b_used; i++)
    s->alloc->free(s->blkpools[i].blks);

  /* delete the block pool pointers */

  s->alloc->free(s->blkpools);

  s->alloc->free(s);
}
