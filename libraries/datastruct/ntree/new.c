/* --------------------------------------------------------------------------
 *    Name: new.c
 * Purpose: N-ary tree
 * ----------------------------------------------------------------------- */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"

#include "datastruct/ntree.h"

#include "impl.h"

result_t ntree_new(ntree_t **t)
{
  ntree_t *n;

  n = calloc(1, sizeof(*n));
  if (n == NULL)
    return result_OOM;

  *t = n;

  return result_OK;
}
