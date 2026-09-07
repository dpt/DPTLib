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

/* TOGGLE_SIZE is the rightmost titlebar icon, so its hit box owns the top-right
 * corner: it grows right and up to the window edge. Its left edge (facing the
 * title text) is unchanged. */
void wuss__toggle_hit_box(const wuss_window_t *window, box_t *out)
{
  box_t drawn;

  wuss__toggle_box(window, &drawn);

  out->x0 = drawn.x0;
  out->y0 = window->visible.y0;
  out->x1 = window->visible.x1;
  out->y1 = drawn.y1;
}
