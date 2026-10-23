/* datastruct/ntree/append.c -- n-ary tree */

#include <stdlib.h>

#include "datastruct/ntree.h"

#include "impl.h"

result_t ntree_append(ntree_t *parent, ntree_t *node)
{
  return ntree_insert_before(parent, NULL, node);
}
