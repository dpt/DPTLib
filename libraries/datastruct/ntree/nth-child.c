/* --------------------------------------------------------------------------
 *    Name: nth-child.c
 * Purpose: N-ary tree
 * ----------------------------------------------------------------------- */

#include "datastruct/ntree.h"

#include "impl.h"

ntree_t *ntree_nth_child(ntree_t *t, int n)
{
  t = t->children;
  if (t)
    while (n-- > 0 && t)
      t = t->next;

  return t;
}
