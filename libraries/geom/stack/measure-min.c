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
    }
    else if (items[i].kind == stack_KIND_SPACER)
    {
      child_main  = 0;
      child_cross = 0;
    }
    else /* stack_KIND_LEAF */
    {
      child_main  = items[i].axis_size ? items[i].axis_size : items[i].min;
      child_cross = items[i].cross_size;
    }

    main_min += child_main + (nchildren > 0 ? items[index].gap : 0);
    cross_min = MAX(cross_min, child_cross);
    nchildren++;
  }

  main_min = MAX(main_min, items[index].axis_size ? items[index].axis_size : items[index].min);

  if (horiz)
  {
    mins[index].w = main_min  + items[index].pad_l + items[index].pad_r;
    mins[index].h = cross_min + items[index].pad_t + items[index].pad_b;
  }
  else
  {
    mins[index].w = cross_min + items[index].pad_l + items[index].pad_r;
    mins[index].h = main_min  + items[index].pad_t + items[index].pad_b;
  }
}

result_t stack_smallest(const stack_item_t *items,
                        int                 n,
                        size2d_t           *out)
{
  size2d_t mins[n];

  if (!stack__valid_tree(items, n))
    return result_STACK_BAD_TREE;

  stack__measure(items, n, 0, mins);
  *out = mins[0];

  return result_OK;
}
