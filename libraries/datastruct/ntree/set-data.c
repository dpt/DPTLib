/* set-data.c -- n-ary tree */

#include "datastruct/ntree.h"

#include "impl.h"

void ntree_set_data(ntree_t *t, void *data)
{
  t->data = data;
}
