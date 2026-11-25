/* datastruct/ntree/new.c -- n-ary tree */

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"

#include "datastruct/ntree.h"

#include "impl.h"

result_t ntree_new(const alloc_t *alloc, ntree_t **t)
{
  ntree_t *n;

  if (alloc == NULL)
    alloc = &alloc_default;

  n = alloc->malloc(sizeof(*n));
  if (n == NULL)
    return result_OOM;

  memset(n, 0, sizeof(*n));
  n->alloc = alloc;

  *t = n;

  return result_OK;
}
