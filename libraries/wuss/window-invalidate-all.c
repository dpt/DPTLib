/* window-invalidate-all.c -- wuss - minimal window manager */

#include "impl.h"

void wuss_window_invalidate_all(wuss_window_t *window)
{
  box_t content;

  wuss_window_get_content_bounds(window, &content);
  content.x1 -= content.x0; content.x0 = 0;
  content.y1 -= content.y0; content.y0 = 0;

  wuss_window_invalidate(window, &content);
}
