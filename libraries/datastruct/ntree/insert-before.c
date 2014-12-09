/* insert-before.c -- n-ary tree */

#include <assert.h>
#include <stdlib.h>

#include "datastruct/ntree.h"

#include "impl.h"

result_t ntree_insert_before(ntree_t *parent, ntree_t *sibling, ntree_t *node)
{
  assert(node->parent == NULL);

  node->parent = parent;

  if (sibling)
  {
    if (sibling->prev)
    {
      node->prev = sibling->prev;
      node->prev->next = node;
      node->next = sibling;
      sibling->prev = node;
    }
    else
    {
      parent->children = node; // or node->parent->children = node;
      node->next = sibling;
      sibling->prev = node;
    }
  }
  else
  {
    /* not given a sibling */

    if (parent->children)
    {
      /* insert at end */
      sibling = parent->children;
      while (sibling->next)
        sibling = sibling->next;
      node->prev = sibling;
      sibling->next = node;
    }
    else
    {
      parent->children = node;
    }
  }

  return result_OK;
}
