/* datastruct/atom/create.c -- atoms */

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "utils/barith.h"

#include "datastruct/atom.h"

#include "impl.h"

atom_set_t *atom_create(const alloc_t *alloc)
{
  return atom_create_tuned(alloc, 0, 0); /* use default values */
}

atom_set_t *atom_create_tuned(const alloc_t *alloc,
                              size_t         locpoolsz,
                              size_t         blkpoolsz)
{
  atom_set_t *s;

  if (alloc == NULL)
    alloc = &alloc_default;

  s = alloc->malloc(sizeof(*s));
  if (s == NULL)
    return NULL;

  memset(s, 0, sizeof(*s));
  s->alloc = alloc;

  s->log2locpoolsz = locpoolsz ? ceillog2_size_t(locpoolsz) : LOG2LOCPOOLSZ;
  s->log2blkpoolsz = blkpoolsz ? ceillog2_size_t(blkpoolsz) : LOG2BLKPOOLSZ;

  return s;
}
