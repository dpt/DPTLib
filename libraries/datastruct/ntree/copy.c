/* copy.c -- n-ary tree */

#include <stdlib.h>

#include "base/result.h"
#include "datastruct/ntree.h"

#include "impl.h"

result_t ntree_copy(ntree_t         *t,
                    ntree_copy_fn_t *fn,
                    void            *opaque,
                    ntree_t        **new_t)
{
  result_t err;
  void    *data;
  ntree_t *new_node;
  ntree_t *child;

  if (!t)
    return result_BAD_ARG;

  err = fn(t->data, opaque, &data);
  if (err)
    return err;

  err = ntree_new(&new_node);
  if (err)
    return err;

  ntree_set_data(new_node, data);

  /* we walk the list of children backwards
   * so we can prepend, which takes constant time */

  for (child = ntree_last_child(t); child; child = child->prev)
  {
    ntree_t *new_child;

    err = ntree_copy(child, fn, opaque, &new_child);
    if (err)
      return err;

    err = ntree_prepend(new_node, new_child);
    if (err)
      return err;
  }

  *new_t = new_node;

  return err;
}
