/* --------------------------------------------------------------------------
 *    Name: impl.h
 * Purpose: N-ary tree
 * ----------------------------------------------------------------------- */

#ifndef NTREE_IMPL_H
#define NTREE_IMPL_H

#include <stdlib.h>

#include "datastruct/ntree.h"

struct ntree_t
{
  ntree_t *next;
  ntree_t *prev;
  ntree_t *parent;
  ntree_t *children;
  void    *data;
};

#define IS_ROOT(t) ((t)->parent == NULL && \
                    (t)->prev   == NULL && \
                    (t)->next   == NULL)

#endif /* NTREE_IMPL_H */
