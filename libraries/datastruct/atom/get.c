/* --------------------------------------------------------------------------
 *    Name: get.c
 * Purpose: Atoms
 * ----------------------------------------------------------------------- */

#include <assert.h>
#include <stddef.h>

#include "datastruct/atom.h"

#include "impl.h"

const unsigned char *atom_get(atom_set_t *s, atom_t a, size_t *plength)
{
  int length;

  assert(s);
  assert(a != atom_NOT_FOUND);

  if (!s->locpools)
    return NULL; /* empty set */

  assert(s->l_used >= 1);

  if (!ATOMVALID(a))
    return NULL; /* out of range */

  length = ATOMLENGTH(a);
  if (length < 0)
    return NULL; /* deleted */

  if (plength)
    *plength = length;

  return ATOMPTR(a);
}
