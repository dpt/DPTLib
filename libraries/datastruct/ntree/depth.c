/* depth.c -- n-ary tree */

#include "datastruct/ntree.h"

#include "impl.h"

int ntree_depth(ntree_t *t)
{
  int d;

  d = 0;

  while (t)
  {
    d++;
    t = t->parent;
  }

  return d;
}
