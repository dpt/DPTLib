/* prepend.c -- n-ary tree */

#include <stdlib.h>

#include "datastruct/ntree.h"

#include "impl.h"

result_t ntree_prepend(ntree_t *parent, ntree_t *node)
{
  return ntree_insert_before(parent, parent->children, node);
}
