/* wuss/window/at.c -- wuss - minimal window manager */

#include "../impl.h"

wuss_window_t *wuss__window_at(wuss_t *wuss, point_t p)
{
  wuss_window_t *win;

  for (win = wuss__z_first(wuss); win != NULL; win = wuss__z_below(win))
  {
    if (win->flags & wuss_WINDOW_HIDDEN)
      continue;
    if (box_contains_point(&win->visible, p.x, p.y))
      return win;
  }

  return NULL;
}
