/* free.c -- n-ary tree */

#include <assert.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/ntree.h"

#include "impl.h"

void ntree_free(ntree_t *t)
{
  ntree_t *next;

  assert(t);

  for (; t; t = next)
  {
    next = t->next;

    if (t->children)
      ntree_free(t->children);

    free(t);
  }
}
