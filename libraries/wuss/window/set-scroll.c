/* set-scroll.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss_window_set_scroll(wuss_window_t *window, int x, int y)
{
  box_t content;

  window->scroll_x = x;
  window->scroll_y = y;

  wuss__content_box(window, &content);
  wuss__invalidate_clipped(window, &content);
}
