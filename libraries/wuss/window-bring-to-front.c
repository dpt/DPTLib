/* window-bring-to-front.c -- wuss - minimal window manager */

#include "impl.h"

void wuss_window_bring_to_front(wuss_window_t *window)
{
  if (window->wuss->z_order.next == &window->link)
    return; /* already at front */

  list_remove(&window->wuss->z_order, &window->link);
  list_add_to_head(&window->wuss->z_order, &window->link);

  wuss_invalidate(window->wuss, &window->visible);
}
