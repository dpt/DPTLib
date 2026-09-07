/* wuss/furniture/back-box.c -- wuss - minimal window manager */

#include "../core/impl.h"

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

/* BACK is the leftmost titlebar icon, so its hit box owns the top-left corner:
 * it grows left and up to the window edge, swallowing the outline pixel and the
 * inset strip between the outline and the drawn icon. Its right and bottom
 * edges (facing CLOSE and the titlebar) are unchanged. */
void wuss__back_hit_box(const wuss_window_t *window, box_t *out)
{
  box_t drawn;

  wuss__back_box(window, &drawn);

  out->x0 = window->visible.x0;
  out->y0 = window->visible.y0;
  out->x1 = drawn.x1;
  out->y1 = drawn.y1;
}
