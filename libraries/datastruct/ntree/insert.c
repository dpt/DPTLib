/* --------------------------------------------------------------------------
 *    Name: insert.c
 * Purpose: N-ary tree
 * ----------------------------------------------------------------------- */

#include <stdlib.h>

#include "datastruct/ntree.h"

#include "impl.h"

result_t ntree_insert(ntree_t *parent, int where, ntree_t *node)
{
  ntree_t *sibling;

  if (where > 0)
    sibling = ntree_nth_child(parent, where);
  else if (where == 0) /* prepend */
    sibling = parent->children;
  else /* append */
    sibling = NULL;

  return ntree_insert_before(parent, sibling, node);
}
