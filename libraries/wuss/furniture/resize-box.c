/* wuss/furniture/resize-box.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss__resize_box(const wuss_window_t *window, box_t *out)
{
  int outline_px, size;

  outline_px = wuss__outline_px(window);
  size       = wuss__button_size(window);

  out->x1 = window->visible.x1 - outline_px;
  out->x0 = out->x1 - size;
  out->y1 = window->visible.y1 - outline_px;
  out->y0 = out->y1 - size;
}
