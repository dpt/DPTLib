/* --------------------------------------------------------------------------
 *    Name: n-nodes.c
 * Purpose: N-ary tree
 * ----------------------------------------------------------------------- */

#include "base/suppress.h"

#include "datastruct/ntree.h"

#include "impl.h"

static result_t ntree_count_func(ntree_t *t, void *opaque)
{
  int *n;

  NOT_USED(t);

  n = opaque;

  (*n)++;

  return result_OK;
}

int ntree_n_nodes(ntree_t *t)
{
  int n;

  n = 0;

  ntree_walk(t,
             ntree_WALK_IN_ORDER | ntree_WALK_ALL,
             0,
             ntree_count_func,
   (void *) &n);

  return n;
}
