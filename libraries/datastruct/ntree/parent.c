/* parent.c -- n-ary tree */

#include <stdlib.h>

#include "datastruct/ntree.h"

#include "impl.h"

ntree_t *ntree_parent(ntree_t *t)
{
  return t ? t->parent : NULL;
}
