/* wuss/window/set-hidden.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss_window_set_hidden(wuss_window_t *window, int hidden)
{
  int was_hidden;

  was_hidden = (window->flags & wuss_WINDOW_HIDDEN) != 0;
  if (!was_hidden == !hidden)
    return; /* no change */

  if (hidden)
  {
    /* still on screen: repaint its footprint now, then mark it gone */
    wuss_invalidate(window->wuss, &window->visible);
    window->flags |= wuss_WINDOW_HIDDEN;
  }
  else
  {
    /* mark it back, then repaint its footprint so it appears */
    window->flags &= (wuss_window_flags_t) ~wuss_WINDOW_HIDDEN;
    wuss_invalidate(window->wuss, &window->visible);
  }
}
