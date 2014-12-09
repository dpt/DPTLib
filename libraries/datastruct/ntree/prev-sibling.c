/* prev-sibling.c -- n-ary tree */

#include <stdlib.h>

#include "datastruct/ntree.h"

#include "impl.h"

ntree_t *ntree_prev_sibling(ntree_t *t)
{
  return t ? t->prev : NULL;
}
