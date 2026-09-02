/* wuss/furniture/titlebar-box.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss__titlebar_box(const wuss_window_t *window, box_t *out)
{
  int outline_px;

  outline_px = wuss__outline_px(window);

  out->x0 = window->visible.x0 + outline_px;
  out->y0 = window->visible.y0 + outline_px;
  out->x1 = window->visible.x1 - outline_px;
  out->y1 = out->y0 + wuss__titlebar_height(window);
}
