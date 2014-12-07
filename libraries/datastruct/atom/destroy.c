/* --------------------------------------------------------------------------
 *    Name: destroy.c
 * Purpose: Atoms
 * ----------------------------------------------------------------------- */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/atom.h"

#include "impl.h"

void atom_destroy(atom_set_t *s)
{
  int i;

  if (s == NULL)
    return;

  /* delete all location pools */

  for (i = 0; i < s->l_used; i++)
    free(s->locpools[i].locs);

  /* delete the location pool pointers */

  free(s->locpools);

  /* delete all block pools */

  for (i = 0; i < s->b_used; i++)
    free(s->blkpools[i].blks);

  /* delete the block pool pointers */

  free(s->blkpools);

  free(s);
}
