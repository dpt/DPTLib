/* window-send-to-back.c -- wuss - minimal window manager */

#include "impl.h"

void wuss_window_send_to_back(wuss_window_t *window)
{
  if (window->link.next == NULL)
    return; /* already at back */

  list_remove(&window->wuss->z_order, &window->link);
  list_add_to_tail(&window->wuss->z_order, &window->link);

  /* now that the reorder has put other windows above it, this invalidates
   * exactly the parts of its footprint they cover -- the same occlusion
   * calculation wuss__invalidate_uncovered does for bring-to-front, just
   * run after the reorder instead of before, so "hidden" now means
   * "newly hidden" */
  wuss__invalidate_uncovered(window);
}
