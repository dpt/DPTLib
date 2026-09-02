/* wuss/furniture/content-box.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss__content_box(const wuss_window_t *window, box_t *out)
{
  int     outline_px;
  point_t carve;

  outline_px = wuss__outline_px(window);

  out->x0 = window->visible.x0 + outline_px;
  out->y0 = window->visible.y0 + outline_px + wuss__titlebar_height(window);
  out->x1 = window->visible.x1 - outline_px;
  out->y1 = window->visible.y1 - outline_px;

  wuss__furniture_carve_for(window->flags, wuss__button_size(window), &carve);
  out->x1 -= carve.x;
  out->y1 -= carve.y;
}
