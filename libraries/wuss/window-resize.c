/* window-resize.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_window_resize(wuss_window_t *window, int width, int height)
{
  box_t before, dirty;

  if (!wuss__size_ok(width, height, wuss__titlebar_height(window)))
    return result_WUSS_TOO_SMALL;

  before = window->visible;

  window->visible.x1 = window->visible.x0 + width;
  window->visible.y1 = window->visible.y0 + height;

  box_union(&before, &window->visible, &dirty);
  wuss_invalidate(window->wuss, &dirty);

  return result_OK;
}
