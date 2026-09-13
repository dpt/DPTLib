/* geom/stack/solve.c -- box-stack layout solver */

#include <stddef.h>

#include "base/utils.h"
#include "geom/box.h"

#include "geom/stack.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

/* A child's resolved main-axis extent, and the room left to grow it. */
typedef struct stack__child
{
  int index;
  int extent;
  int clamped; /* non-zero once this child can no longer grow */
}
stack__child_t;

/* ----------------------------------------------------------------------- */

/* Distributes `avail` px of main-axis space across the children of
 * `parent`, honouring `size`/`min`/`flex`/`max`, and returns the number of
 * children found. `children[i].extent` holds each child's resolved
 * main-axis extent on return. */
static int stack__distribute(const stack_item_t *items,
                             int                 n,
                             int                 parent,
                             int                 avail,
                             stack__child_t     *children)
{
  int            nchildren;
  int            used;
  int            slack;
  int            i;
  int            round;

  nchildren = 0;
  used      = 0;

  for (i = 0; i < n; i++)
  {
    if (items[i].parent != parent)
      continue;

    children[nchildren].index   = i;
    children[nchildren].extent  = items[i].size ? items[i].size : items[i].min;
    children[nchildren].clamped = (items[i].flex == 0);

    used += children[nchildren].extent;
    nchildren++;
  }

  slack = avail - used;

  /* Hand slack to flexible children in proportion to their weights, then
   * the leftover px from integer division one per child, earliest first,
   * so extents sum to exactly 'avail'. Clamping a child to its 'max'
   * returns its excess to 'slack' for another such pass over the children
   * still able to grow; bounded by 'nchildren' passes, as each pass
   * clamps at least one more child or terminates. */
  for (round = 0; round < nchildren && slack > 0; round++)
  {
    int flexweight;
    int given;
    int c;

    flexweight = 0;
    for (c = 0; c < nchildren; c++)
      if (!children[c].clamped)
        flexweight += items[children[c].index].flex;

    if (flexweight == 0)
      break;

    given = 0;
    for (c = 0; c < nchildren; c++)
    {
      int share;

      if (children[c].clamped)
        continue;

      share = slack * items[children[c].index].flex / flexweight;
      children[c].extent += share;
      given               += share;
    }

    for (c = 0; c < nchildren && given < slack; c++)
    {
      if (children[c].clamped)
        continue;
      children[c].extent++;
      given++;
    }

    for (c = 0; c < nchildren; c++)
    {
      int item_max;

      if (children[c].clamped)
        continue;

      item_max = items[children[c].index].max;
      if (item_max > 0 && children[c].extent > item_max)
      {
        given -= children[c].extent - item_max;
        children[c].extent = item_max;
        children[c].clamped = 1;
      }
    }

    slack -= given;
  }

  return nchildren;
}

/* ----------------------------------------------------------------------- */

static void stack__place_container(const stack_item_t *items,
                                   int                 n,
                                   int                 index,
                                   box_t              *out)
{
  int                horiz;
  stack__child_t      children[n]; /* upper bound: at most n-1 children */
  int                nchildren;
  const box_t       *box;
  int                inner_main0, inner_main1;
  int                cross0, cross1;
  int                avail;
  int                pos;
  int                c;

  horiz = (items[index].kind == stack_KIND_HBOX);
  box   = &out[index];

  if (horiz)
  {
    inner_main0 = box->x0 + items[index].pad_l;
    inner_main1 = box->x1 - items[index].pad_r;
    cross0      = box->y0 + items[index].pad_t;
    cross1      = box->y1 - items[index].pad_b;
  }
  else
  {
    inner_main0 = box->y0 + items[index].pad_t;
    inner_main1 = box->y1 - items[index].pad_b;
    cross0      = box->x0 + items[index].pad_l;
    cross1      = box->x1 - items[index].pad_r;
  }

  nchildren = 0;
  for (c = 0; c < n; c++)
    if (items[c].parent == index)
      nchildren++;

  avail = inner_main1 - inner_main0 - items[index].gap * MAX(0, nchildren - 1);

  nchildren = stack__distribute(items, n, index, avail, children);

  pos = inner_main0;

  for (c = 0; c < nchildren; c++)
  {
    int          ci;
    box_t       *cbox;
    int          cross_extent;
    int          cross_pos;

    ci   = children[c].index;
    cbox = &out[ci];

    if (c > 0)
      pos += items[index].gap;

    switch (items[ci].align)
    {
    case stack_ALIGN_FILL:
      cross_extent = cross1 - cross0;
      cross_pos    = cross0;
      break;
    case stack_ALIGN_CENTRE:
      cross_extent = items[ci].size ? items[ci].size : (cross1 - cross0);
      cross_pos    = cross0 + (cross1 - cross0 - cross_extent) / 2;
      break;
    case stack_ALIGN_END:
      cross_extent = items[ci].size ? items[ci].size : (cross1 - cross0);
      cross_pos    = cross1 - cross_extent;
      break;
    case stack_ALIGN_START:
    default:
      cross_extent = items[ci].size ? items[ci].size : (cross1 - cross0);
      cross_pos    = cross0;
      break;
    }

    if (horiz)
    {
      cbox->x0 = pos;
      cbox->x1 = pos + children[c].extent;
      cbox->y0 = cross_pos;
      cbox->y1 = cross_pos + cross_extent;
    }
    else
    {
      cbox->y0 = pos;
      cbox->y1 = pos + children[c].extent;
      cbox->x0 = cross_pos;
      cbox->x1 = cross_pos + cross_extent;
    }

    pos += children[c].extent;

    if (items[ci].kind == stack_KIND_HBOX || items[ci].kind == stack_KIND_VBOX)
      stack__place_container(items, n, ci, out);
  }
}

/* ----------------------------------------------------------------------- */

result_t stack_solve(const stack_item_t *items,
                     int                 n,
                     const box_t        *root,
                     box_t              *out)
{
  if (!stack__valid_tree(items, n))
    return result_STACK_BAD_TREE;

  out[0] = *root;

  if (items[0].kind == stack_KIND_HBOX || items[0].kind == stack_KIND_VBOX)
    stack__place_container(items, n, 0, out);

  return result_OK;
}
