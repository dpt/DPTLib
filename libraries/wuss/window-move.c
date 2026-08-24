/* window-move.c -- wuss - minimal window manager */

#include "impl.h"

void wuss_window_move(wuss_window_t *window, int x, int y)
{
  int   width, height;
  box_t before, dirty;

  width  = window->visible.x1 - window->visible.x0;
  height = window->visible.y1 - window->visible.y0;
  before = window->visible;

  window->visible.x0 = x;
  window->visible.y0 = y;
  window->visible.x1 = x + width;
  window->visible.y1 = y + height;

  box_union(&before, &window->visible, &dirty);
  wuss_invalidate(window->wuss, &dirty);
}
