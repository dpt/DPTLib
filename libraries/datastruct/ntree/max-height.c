/* datastruct/ntree/max-height.c -- n-ary tree */

#include "base/utils.h"
#include "datastruct/ntree.h"

#include "impl.h"

int ntree_max_height(ntree_t *t)
{
  int      max;
  ntree_t *child;

  max = 0;

  if (!t)
    return max;

  for (child = t->children; child; child = child->next)
  {
    int h;

    h   = ntree_max_height(child);
    max = MAX(max, h);
  }

  return max + 1;
}
