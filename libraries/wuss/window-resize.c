/* window-resize.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_window_resize(wuss_window_t *window, int width, int height)
{
  int   outline_px, titlebar_height;
  box_t before, dirty;

  if (!wuss__size_ok(width, height))
    return result_WUSS_TOO_SMALL;

  outline_px      = wuss__outline_px(window);
  titlebar_height = wuss__titlebar_height(window);
  before          = window->visible;

  window->visible.x1 = window->visible.x0 + width  + 2 * outline_px;
  window->visible.y1 = window->visible.y0 + height + titlebar_height + 2 * outline_px;

  box_union(&before, &window->visible, &dirty);
  wuss__invalidate_clipped(window, &dirty);

  return result_OK;
}
