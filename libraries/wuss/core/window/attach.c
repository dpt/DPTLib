/* wuss/window/attach.c -- attach toolbar windows to a parent window */

#include <assert.h>
#include <stddef.h>

#include "base/utils.h"

#include "wuss/window.h"

#include "../impl.h"

/* Furniture a toolbar may have: nothing the user could move, resize or
 * close it with. */
#define TOOLBAR_FLAGS (wuss_WINDOW_NO_TITLEBAR    | wuss_WINDOW_NO_OUTLINE  | \
                       wuss_WINDOW_VSCROLL        | wuss_WINDOW_HSCROLL     | \
                       wuss_WINDOW_NO_RESIZE_BLIT | wuss_WINDOW_HIDDEN      | \
                       wuss_WINDOW_NO_REDRAW      | wuss_WINDOW_FOCUSABLE)

/* ----------------------------------------------------------------------- */

/* Non-zero if "toolbar" takes room in its edge's stack. */
static int takes_room(const wuss_window_t *toolbar,
                      const wuss_window_t *reveal)
{
  return !(toolbar->flags & wuss_WINDOW_HIDDEN) || toolbar == reveal;
}

/* Lay out the toolbars of "parent" on one edge. Along the edge each is
 * aligned against the parent's content area; across it they stack outwards
 * from the parent's footprint. The whole stack is clamped as one block so
 * that it stays on the screen, overlapping the parent if needs be, but once
 * the parent's edge itself is off the screen the block goes with it. */
static void layout_edge(wuss_window_t *parent,
                        wuss_window_t *reveal,
                        wuss_edge_t    edge)
{
  wuss_window_t *tb;
  int            horizontal;
  box_t          content;
  int            total, start, used, thick, length, along;
  int            screen_far, near, far;
  box_t          tb_content;
  point_t        p;

  horizontal = (edge == wuss_EDGE_LEFT || edge == wuss_EDGE_RIGHT);
  wuss__content_box(parent, &content);

  total = 0;
  for (tb = parent->toolbar.first; tb != NULL; tb = tb->toolbar.next)
    if (tb->toolbar.edge == edge && takes_room(tb, reveal))
      total += horizontal ? tb->visible.x1 - tb->visible.x0
                          : tb->visible.y1 - tb->visible.y0;

  if (horizontal)
  {
    near       = parent->visible.x0;
    far        = parent->visible.x1;
    screen_far = parent->wuss->scr->size.w;
  }
  else
  {
    near       = parent->visible.y0;
    far        = parent->visible.y1;
    screen_far = parent->wuss->scr->size.h;
  }

  /* outer start of the block on a near edge, inner start on a far one */
  if (edge == wuss_EDGE_LEFT || edge == wuss_EDGE_TOP)
    start = MAX(near - total, MIN(0, near));
  else
    start = MAX(MIN(far, screen_far - total), far - total);

  used = 0;
  for (tb = parent->toolbar.first; tb != NULL; tb = tb->toolbar.next)
  {
    if (tb->toolbar.edge != edge || !takes_room(tb, reveal))
      continue;

    if (horizontal)
    {
      thick  = tb->visible.x1 - tb->visible.x0;
      length = tb->visible.y1 - tb->visible.y0;
      if (tb->toolbar.align & wuss_ALIGN_TOP)
        along = content.y0;
      else if (tb->toolbar.align & wuss_ALIGN_BOTTOM)
        along = content.y1 - length;
      else
        along = (content.y0 + content.y1 - length) / 2;
    }
    else
    {
      thick  = tb->visible.y1 - tb->visible.y0;
      length = tb->visible.x1 - tb->visible.x0;
      if (tb->toolbar.align & wuss_ALIGN_LEFT)
        along = content.x0;
      else if (tb->toolbar.align & wuss_ALIGN_RIGHT)
        along = content.x1 - length;
      else
        along = (content.x0 + content.x1 - length) / 2;
    }
    along += tb->toolbar.offset;

    /* p is the toolbar's footprint top-left; first attached is innermost */
    if (edge == wuss_EDGE_LEFT || edge == wuss_EDGE_TOP)
      p.x = start + total - used - thick;
    else
      p.x = start + used;
    p.y = along;
    if (!horizontal)
      SWAP(p.x, p.y);
    used += thick;

    wuss__content_box(tb, &tb_content);
    wuss__window_move(tb, POINT(p.x + tb_content.x0 - tb->visible.x0,
                                p.y + tb_content.y0 - tb->visible.y0));
  }
}

void wuss__toolbars_layout(wuss_window_t *parent, wuss_window_t *reveal)
{
  int edge;

  if (parent->toolbar.first == NULL)
    return;

  for (edge = wuss_EDGE_LEFT; edge <= wuss_EDGE_BOTTOM; edge++)
    layout_edge(parent, reveal, (wuss_edge_t) edge);
}

/* ----------------------------------------------------------------------- */

/* Relink "toolbar" into the parent's stack, directly above it. */
static void link_above(wuss_window_t *toolbar, wuss_window_t *parent)
{
  wuss_t *wuss;
  list_t *prev;

  wuss = parent->wuss;

  list_remove(&wuss->z_order[toolbar->stack], &toolbar->link);
  toolbar->stack = parent->stack;

  for (prev = &wuss->z_order[parent->stack];
       prev->next != &parent->link;
       prev = prev->next)
    ;

  toolbar->link.next = &parent->link;
  prev->next         = &toolbar->link;
}

result_t wuss_window_attach(wuss_window_t *toolbar,
                            wuss_window_t *parent,
                            wuss_edge_t    edge,
                            wuss_align_t   align,
                            int            offset)
{
  wuss_window_t **tail;
  int             hidden;

  assert(toolbar != NULL);
  assert(parent  != NULL);
  assert((int) edge >= wuss_EDGE_LEFT && (int) edge <= wuss_EDGE_BOTTOM);

  if (toolbar == parent                  ||
      toolbar->task != parent->task      ||
      toolbar->toolbar.parent != NULL    ||
      toolbar->toolbar.first  != NULL    ||
      parent->toolbar.parent  != NULL    ||
      !(toolbar->flags & wuss_WINDOW_NO_TITLEBAR) ||
      (toolbar->flags & ~TOOLBAR_FLAGS) != 0)
    return result_BAD_ARG;

  hidden = (toolbar->flags & wuss_WINDOW_HIDDEN) != 0;

  /* repaint wherever it was; it is painted afresh in place below */
  if (!hidden)
    wuss__invalidate_clipped(toolbar, &toolbar->visible);

  toolbar->toolbar.parent = parent;
  toolbar->toolbar.next   = NULL;
  toolbar->toolbar.edge   = edge;
  toolbar->toolbar.align  = align;
  toolbar->toolbar.offset = offset;

  for (tail = &parent->toolbar.first; *tail != NULL;
       tail = &(*tail)->toolbar.next)
    ;
  *tail = toolbar;

  link_above(toolbar, parent);
  wuss__toolbars_layout(parent, NULL);

  if (!hidden)
    wuss__invalidate_clipped(toolbar, &toolbar->visible);

  return result_OK;
}

void wuss_window_detach(wuss_window_t *toolbar)
{
  wuss_window_t  *parent;
  wuss_window_t **link;

  parent = toolbar->toolbar.parent;
  if (parent == NULL)
    return;

  for (link = &parent->toolbar.first; *link != toolbar;
       link = &(*link)->toolbar.next)
    ;
  *link = toolbar->toolbar.next;

  toolbar->toolbar.parent = NULL;
  toolbar->toolbar.next   = NULL;
  toolbar->state &= (wuss_window_state_t) ~wuss_WINDOW_STATE_PARENT_HIDDEN;

  wuss__toolbars_layout(parent, NULL);
}
