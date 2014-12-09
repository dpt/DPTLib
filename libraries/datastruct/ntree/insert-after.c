/* insert-after.c -- n-ary tree */

#include <assert.h>
#include <stdlib.h>

#include "base/suppress.h"

#include "base/result.h"
#include "datastruct/ntree.h"

#include "impl.h"

result_t ntree_insert_after(ntree_t *parent, ntree_t *sibling, ntree_t *node)
{
  NOT_USED(parent);
  NOT_USED(sibling);
  NOT_USED(node);

  assert("NYI" == NULL);

  return result_OK;
}
