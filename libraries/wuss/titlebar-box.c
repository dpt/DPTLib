/* titlebar-box.c -- wuss - minimal window manager */

#include "impl.h"

void wuss__titlebar_box(const wuss_window_t *window, box_t *out)
{
  out->x0 = window->visible.x0;
  out->y0 = window->visible.y0;
  out->x1 = window->visible.x1;
  out->y1 = window->visible.y0 + wuss__titlebar_height(window);
}
