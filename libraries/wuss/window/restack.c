/* restack.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss_window_restack(wuss_window_t *window, wuss_zorder_t reason)
{
  if (reason == wuss_ZORDER_FRONT)
  {
    if (window->wuss->z_order.next == &window->link)
      return; /* already at front */

    wuss__invalidate_uncovered(window);

    list_remove(&window->wuss->z_order, &window->link);
    list_add_to_head(&window->wuss->z_order, &window->link);
  }
  else
  {
    if (window->link.next == NULL)
      return; /* already at back */

    list_remove(&window->wuss->z_order, &window->link);
    list_add_to_tail(&window->wuss->z_order, &window->link);

    /* now that the reorder has put other windows above it, this invalidates
     * exactly the parts of its footprint they cover -- the same occlusion
     * calculation as the front case, just run after the reorder instead of
     * before, so "hidden" now means "newly hidden" */
    wuss__invalidate_uncovered(window);
  }
}
