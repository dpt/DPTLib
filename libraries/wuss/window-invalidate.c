/* window-invalidate.c -- wuss - minimal window manager */

#include "impl.h"

void wuss_window_invalidate(wuss_window_t *window, const box_t *local_box)
{
  box_t screen_box;
  int   titlebar_height;

  titlebar_height = wuss__titlebar_height(window);

  screen_box.x0 = window->visible.x0 + local_box->x0;
  screen_box.y0 = window->visible.y0 + titlebar_height + local_box->y0;
  screen_box.x1 = window->visible.x0 + local_box->x1;
  screen_box.y1 = window->visible.y0 + titlebar_height + local_box->y1;

  wuss_invalidate(window->wuss, &screen_box);
}
