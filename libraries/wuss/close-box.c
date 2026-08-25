/* close-box.c -- wuss - minimal window manager */

#include "impl.h"

#define WUSS_CLOSE_ICON_INSET 3

void wuss__close_box(const wuss_window_t *window, box_t *out)
{
  box_t titlebar;
  int   inset, size;

  wuss__titlebar_box(window, &titlebar);

  inset = WUSS_CLOSE_ICON_INSET;
  size  = (titlebar.y1 - titlebar.y0) - inset * 2;

  out->x0 = titlebar.x0 + inset;
  out->y0 = titlebar.y0 + inset;
  out->x1 = out->x0 + size;
  out->y1 = out->y0 + size;
}
