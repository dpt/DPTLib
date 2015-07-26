/* for-block.c -- atoms */

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "datastruct/atom.h"

#include "impl.h"

atom_t atom_for_block(atom_set_t          *s,
                      const unsigned char *block,
                      size_t               sizet_length)
{
  int              length;
  const locpool_t *pend;
  const locpool_t *p;

  assert(s);
  assert(block);
  assert(sizet_length > 0);

  /* The current implementation uses an int for the length of a block, and
   * negative numbers indicate unused blocks, so we cannot cope with lengths
   * greater than INT_MAX even though the interface allows size_t. */
  if (sizet_length > INT_MAX)
    return result_TOO_BIG;

  length = (int) sizet_length;

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
        return (atom_t) (((p - s->locpools) << s->log2locpoolsz) + (l - p->locs));
  }

  return atom_NOT_FOUND;
}
