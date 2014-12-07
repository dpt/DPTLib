/* --------------------------------------------------------------------------
 *    Name: delete.c
 * Purpose: N-ary tree
 * ----------------------------------------------------------------------- */

#include <assert.h>

#include "datastruct/ntree.h"

#include "impl.h"

void ntree_delete(ntree_t *t)
{
  assert(t);

  if (!IS_ROOT(t))
    ntree_unlink(t);

  ntree_free(t);
}
