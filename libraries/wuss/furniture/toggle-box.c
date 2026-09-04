/* wuss/furniture/toggle-box.c -- wuss - minimal window manager */

#include "../core/impl.h"

void wuss__toggle_box(const wuss_window_t *window, box_t *out)
{
  box_t titlebar;
  int   inset, size;

  wuss__titlebar_box(window, &titlebar);

  inset = WUSS_BUTTON_INSET;
  size  = wuss__button_size(window);

  out->x1 = titlebar.x1 - inset;
  out->x0 = out->x1 - size;
  out->y0 = titlebar.y0 + inset;
  out->y1 = out->y0 + size;
}
