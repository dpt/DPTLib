/* resize.c -- wuss - minimal window manager */

#include "../impl.h"

result_t wuss_window_resize(wuss_window_t *window, int width, int height)
{
  int   outline_px, titlebar_height, carve_x, carve_y;
  box_t before, dirty;

  if (!wuss__size_ok(width, height))
    return result_WUSS_TOO_SMALL;

  outline_px      = wuss__outline_px(window);
  titlebar_height = wuss__titlebar_height(window);
  before          = window->visible;
  wuss__furniture_carve_for(window->flags, wuss__icon_size(window), &carve_x, &carve_y);

  window->visible.x1 = window->visible.x0 + width  + 2 * outline_px + carve_x;
  window->visible.y1 = window->visible.y0 + height + titlebar_height + 2 * outline_px + carve_y;

  wuss__notify_open(window);

  box_union(&before, &window->visible, &dirty);
  wuss__invalidate_clipped(window, &dirty);

  return result_OK;
}
