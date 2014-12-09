/* get-data.c -- n-ary tree */

#include "datastruct/ntree.h"

#include "impl.h"

void *ntree_get_data(ntree_t *t)
{
  return t->data;
}
