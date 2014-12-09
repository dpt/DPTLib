/* unlink.c -- n-ary tree */

#include <assert.h>
#include <stdlib.h>

#include "datastruct/ntree.h"

#include "impl.h"

void ntree_unlink(ntree_t *t)
{
  assert(t);

  if (t->prev)
    t->prev->next = t->next;
  else if (t->parent)
    t->parent->children = t->next;

  t->parent = NULL;

  if (t->next)
  {
    t->next->prev = t->prev;
    t->next = NULL;
  }

  t->prev = NULL;
}
