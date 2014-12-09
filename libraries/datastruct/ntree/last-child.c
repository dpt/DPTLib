/* last-child.c -- n-ary tree */

#include "datastruct/ntree.h"

#include "impl.h"

ntree_t *ntree_last_child(ntree_t *t)
{
  t = t->children;
  if (t)
    while (t->next)
      t = t->next;

  return t;
}
