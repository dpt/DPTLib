/* for-block.c -- atoms */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "datastruct/atom.h"

#include "impl.h"

atom_t atom_for_block(atom_set_t          *s,
                      const unsigned char *block,
                      size_t               length)
{
  const locpool_t *pend;
  const locpool_t *p;

  assert(s);
  assert(block);
  assert(length > 0);

  /* linear search every locpool */

  pend = s->locpools + s->l_used;
  for (p = s->locpools; p < pend; p++)
  {
    const loc_t *lend;
    const loc_t *l;

    /* linear search every loc */

    lend = p->locs + p->used;
    for (l = p->locs; l < lend; l++)
      if (l->length == length && memcmp(l->ptr, block, length) == 0)
        return ((p - s->locpools) << s->log2locpoolsz) + (l - p->locs);
  }

  return atom_NOT_FOUND;
}
