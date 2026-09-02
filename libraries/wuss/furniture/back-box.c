/* wuss/furniture/back-box.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss__back_box(const wuss_window_t *window, box_t *out)
{
  box_t titlebar;
  int   inset, size;

  wuss__titlebar_box(window, &titlebar);

  inset = WUSS_BUTTON_INSET;
  size  = wuss__button_size(window);

  out->x0 = titlebar.x0 + inset;
  out->y0 = titlebar.y0 + inset;
  out->x1 = out->x0 + size;
  out->y1 = out->y0 + size;
}
