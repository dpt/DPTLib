/* geom/stack/measure-min.c -- minimum-size measurement for a box-stack tree */

#include "base/utils.h"
#include "geom/box.h"
#include "geom/size.h"

#include "geom/stack.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

/* A leaf's own minimum extent needs no recursion; a container's is derived
 * from its children, recursing depth-first so each child's minimum is known
 * before its parent sums/maxes over it. */
static void stack__measure(const stack_item_t *items,
                           int                 n,
                           int                 index,
                           size2d_t           *mins)
{
  int horiz;
  int main_min, cross_min;
  int nchildren;
  int i;

  horiz = (items[index].kind == stack_KIND_HBOX);

  main_min  = 0;
  cross_min = 0;
  nchildren = 0;

  for (i = 0; i < n; i++)
  {
    int child_main, child_cross;

    if (items[i].parent != index)
      continue;

    if (items[i].kind == stack_KIND_HBOX || items[i].kind == stack_KIND_VBOX)
    {
      stack__measure(items, n, i, mins);
      child_main  = horiz ? mins[i].w : mins[i].h;
      child_cross = horiz ? mins[i].h : mins[i].w;
      /* a fixed axis_size is along this (the parent's) main axis, and
       * stack_solve gives the child exactly that */
      if (items[i].axis_size > 0)
        child_main = items[i].axis_size;
      else
        child_main = MAX(child_main, items[i].min);
    }
    else /* stack_KIND_LEAF or stack_KIND_SPACER */
    {
      child_main  = stack__axis_size(items, n, i);
      if (child_main == 0)
        child_main = items[i].min;
      child_cross = items[i].kind == stack_KIND_SPACER ? 0 : items[i].cross_size;
    }

    main_min += child_main + (nchildren > 0 ? items[index].gap : 0);
    cross_min = MAX(cross_min, child_cross);
    nchildren++;
  }

  if (horiz)
  {
    mins[index].w = main_min  + items[index].pad.l + items[index].pad.r;
    mins[index].h = cross_min + items[index].pad.t + items[index].pad.b;
  }
  else
  {
    mins[index].w = cross_min + items[index].pad.l + items[index].pad.r;
    mins[index].h = main_min  + items[index].pad.t + items[index].pad.b;
  }
}

result_t stack_smallest(const stack_item_t *items,
                        int                 n,
                        size2d_t           *out)
{
  size2d_t mins[STACK_MAX_ITEMS];

  if (n > STACK_MAX_ITEMS)
    return result_STACK_BAD_TREE;
  if (!stack__valid_tree(items, n))
    return result_STACK_BAD_TREE;

  stack__measure(items, n, 0, mins);
  *out = mins[0];

  return result_OK;
}
