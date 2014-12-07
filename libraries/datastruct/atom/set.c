/* --------------------------------------------------------------------------
 *    Name: set.c
 * Purpose: Atoms
 * ----------------------------------------------------------------------- */

#include <assert.h>
#include <string.h>

#include "datastruct/atom.h"

#include "impl.h"

result_t atom_set(atom_set_t          *s,
               atom_t               a,
               const unsigned char *block,
               size_t               length)
{
  result_t   err;
  atom_t  newa;
  loc    *p, *q;
  loc     t;

  assert(s);
  assert(a != atom_NOT_FOUND);
  assert(block);
  assert(length > 0);

  if (!s->locpools)
    return result_ATOM_SET_EMPTY;

  assert(s->l_used >= 1);

  if (!ATOMVALID(a))
    return result_ATOM_OUT_OF_RANGE;

  err = atom_new(s, block, length, &newa);
  if (err == result_ATOM_NAME_EXISTS && newa == a)
    /* setting an atom to its existing data is ignored */
    return result_OK;
  else if (err)
    return err;

  atom_delete(s, a);

  /* now transpose old and new atoms */

  p = &ATOMLOC(a);
  q = &ATOMLOC(newa);

  t  = *p;
  *p = *q;
  *q = t;

  return result_OK;
}
