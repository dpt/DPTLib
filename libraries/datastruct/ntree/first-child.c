/* first-child.c -- n-ary tree */

#include <stdlib.h>

#include "datastruct/ntree.h"

#include "impl.h"

ntree_t *ntree_first_child(ntree_t *t)
{
  return t ? t->children : NULL;
}
