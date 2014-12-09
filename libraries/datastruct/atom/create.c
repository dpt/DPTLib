/* create.c -- atoms */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "utils/barith.h"

#include "datastruct/atom.h"

#include "impl.h"

atom_set_t *atom_create(void)
{
  return atom_create_tuned(0, 0); /* use default values */
}

atom_set_t *atom_create_tuned(size_t locpoolsz, size_t blkpoolsz)
{
  atom_set_t *s;

  s = calloc(1, sizeof(*s));
  if (s == NULL)
    return NULL;

  s->log2locpoolsz = locpoolsz ? ceillog2(locpoolsz) : LOG2LOCPOOLSZ;
  s->log2blkpoolsz = blkpoolsz ? ceillog2(blkpoolsz) : LOG2BLKPOOLSZ;

  return s;
}
