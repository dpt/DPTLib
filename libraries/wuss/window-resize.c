/* window-resize.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_window_resize(wuss_window_t *window, int width, int height)
{
  if (!wuss__size_ok(width, height, window->wuss->titlebar_height))
    return result_WUSS_TOO_SMALL;

  window->visible.x1 = window->visible.x0 + width;
  window->visible.y1 = window->visible.y0 + height;

  return result_OK;
}
