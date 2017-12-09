/* walk.c -- n-ary tree */

#include <assert.h>
#include <stdlib.h>

#include "base/result.h"
#include "datastruct/ntree.h"

#include "impl.h"

static result_t walk_in_order(ntree_t           *t,
                              ntree_walk_flags_t flags,
                              int                depth,
                              ntree_walk_fn_t   *fn,
                              void              *opaque)
{
  result_t err;

  if (t->children)
  {
    ntree_t *sibling;
    ntree_t *next;

    depth--;

    if (depth)
    {
      /* process first child */
      err = walk_in_order(t->children, flags, depth, fn, opaque);
      if (err)
        return err;
    }

    /* then next process the node itself */
    if (flags & ntree_WALK_BRANCHES)
    {
      err = fn(t, opaque);
      if (err)
        return err;
    }

    if (depth)
    {
      /* finally process the remainder of the children */
      for (sibling = t->children->next; sibling; sibling = next)
      {
        next = sibling->next;
        err = walk_in_order(sibling, flags, depth, fn, opaque);
        if (err)
          return err;
      }
    }
  }
  else
  {
    if (flags & ntree_WALK_LEAVES)
    {
      err = fn(t, opaque);
      if (err)
        return err;
    }
  }

  return result_OK;
}

static result_t walk_pre_order(ntree_t           *t,
                               ntree_walk_flags_t flags,
                               int                depth,
                               ntree_walk_fn_t   *fn,
                               void              *opaque)
{
  result_t err;

  if (t->children)
  {
    /* process the node itself */
    if (flags & ntree_WALK_BRANCHES)
    {
      err = fn(t, opaque);
      if (err)
        return err;
    }

    if (--depth)
    {
      ntree_t *sibling;
      ntree_t *next;

      /* finally process the children */
      for (sibling = t->children; sibling; sibling = next)
      {
        next = sibling->next;
        err = walk_pre_order(sibling, flags, depth, fn, opaque);
        if (err)
          return err;
      }
    }
  }
  else
  {
    if (flags & ntree_WALK_LEAVES)
    {
      err = fn(t, opaque);
      if (err)
        return err;
    }
  }

  return result_OK;
}

static result_t walk_post_order(ntree_t           *t,
                                ntree_walk_flags_t flags,
                                int                depth,
                                ntree_walk_fn_t   *fn,
                                void              *opaque)
{
  result_t err;

  if (t->children)
  {
    if (--depth)
    {
      ntree_t *sibling;
      ntree_t *next;

      /* process the children */
      for (sibling = t->children; sibling; sibling = next)
      {
        next = sibling->next;
        err = walk_post_order(sibling, flags, depth, fn, opaque);
        if (err)
          return err;
      }
    }

    /* finally process the node itself */
    if (flags & ntree_WALK_BRANCHES)
    {
      err = fn(t, opaque);
      if (err)
        return err;
    }
  }
  else
  {
    if (flags & ntree_WALK_LEAVES)
    {
      err = fn(t, opaque);
      if (err)
        return err;
    }
  }

  return result_OK;
}

result_t ntree_walk(ntree_t           *t,
                    ntree_walk_flags_t flags,
                    int                max_depth,
                    ntree_walk_fn_t   *fn,
                    void              *opaque)
{
  result_t (*walker)(ntree_t *,
                     ntree_walk_flags_t,
                     int,
                     ntree_walk_fn_t *,
                     void *);

  switch (flags & ntree_WALK_ORDER_MASK)
  {
  default:
  case ntree_WALK_IN_ORDER:
    walker = walk_in_order;
    break;
  case ntree_WALK_PRE_ORDER:
    walker = walk_pre_order;
    break;
  case ntree_WALK_POST_ORDER:
    walker = walk_post_order;
    break;
  }

  return walker(t, flags, max_depth, fn, opaque);
}
