/* window-at.c -- wuss - minimal window manager */

#include "impl.h"

wuss_window_t *wuss__window_at(wuss_t *wuss, int x, int y)
{
  list_t *e;

  for (e = wuss->z_order.next; e != NULL; e = e->next)
  {
    wuss_window_t *win;

    win = (wuss_window_t *) e;
    if (box_contains_point(&win->visible, x, y))
      return win;
  }

  return NULL;
}
