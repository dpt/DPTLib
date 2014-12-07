/* --------------------------------------------------------------------------
 *    Name: delete.c
 * Purpose: Atoms
 * ----------------------------------------------------------------------- */

#include <assert.h>
#include <string.h>

#include "datastruct/atom.h"

#include "impl.h"

/* Delete the specified atom by flipping the sign of the length field. This
 * leaves the data in place but inaccessible. */
void atom_delete(atom_set_t *s, atom_t a)
{
  int *plength;
  int  length;

  assert(s);

  if (a == atom_NOT_FOUND)
    return;

  if (!s->locpools)
    return; /* empty set */

  assert(s->l_used >= 1);

  if (!ATOMVALID(a))
    return; /* out of range */

  plength = &ATOMLENGTH(a);
  length  = *plength;
  if (length < 0)
    return; /* already deleted */

#ifndef NDEBUG
  /* scribble over the deleted copy */
  memset(ATOMPTR(a), 0xAA, length);
#endif

  *plength = -length;
}
