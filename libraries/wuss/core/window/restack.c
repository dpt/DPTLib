/* wuss/window/restack.c -- wuss - minimal window manager */

#include "../impl.h"

static void restack_one(wuss_window_t *window, wuss_zorder_t reason)
{
  list_t *stack;

  stack = &window->wuss->z_order[window->stack];

  if (reason == wuss_ZORDER_FRONT)
  {
    if (stack->next == &window->link)
      return; /* already at front of its stack */

    wuss__invalidate_uncovered(window);

    list_remove(stack, &window->link);
    list_add_to_head(stack, &window->link);
  }
  else
  {
    if (window->link.next == NULL)
      return; /* already at back of its stack */

    list_remove(stack, &window->link);
    list_add_to_tail(stack, &window->link);

    /* now that the reorder has put other windows above it, this invalidates
     * exactly the parts of its footprint they cover -- the same occlusion
     * calculation as the front case, just run after the reorder instead of
     * before, so "hidden" now means "newly hidden" */
    wuss__invalidate_uncovered(window);
  }
}

/* A toolbar group restacks as one: the toolbars stay directly above their
 * parent, the last attached frontmost. */
void wuss_window_restack(wuss_window_t *window, wuss_zorder_t reason)
{
  wuss_window_t *tb;

  window = wuss__toolbar_root(window);

  if (reason == wuss_ZORDER_FRONT)
  {
    restack_one(window, reason);
    for (tb = window->toolbar.first; tb != NULL; tb = tb->toolbar.next)
      restack_one(tb, reason);
  }
  else
  {
    for (tb = window->toolbar.first; tb != NULL; tb = tb->toolbar.next)
      restack_one(tb, reason);
    restack_one(window, reason);
  }
}
