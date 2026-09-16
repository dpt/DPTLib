/* wuss/furniture/close-box.c -- wuss - minimal window manager */

#include "../core/impl.h"

void wuss__close_box(const wuss_window_t *window, box_t *out)
{
  box_t titlebar;
  int   inset, size;

  wuss__titlebar_box(window, &titlebar);

  inset = WUSS_BUTTON_INSET;
  size  = wuss__button_size(window);

  out->x0 = titlebar.x0 + inset;
  if (window->flags & wuss_WINDOW_BACK)
    out->x0 += size + inset; /* shift right, clear of the back icon */
  out->y0 = titlebar.y0 + inset;
  out->x1 = out->x0 + size;
  out->y1 = out->y0 + size;
}

/* CLOSE always grows up over the top outline. It only owns the top-left corner
 * when there is no BACK icon to its left; with BACK present its left edge is
 * unchanged and butts BACK's right edge, so the two hit boxes tile with no
 * seam. */
void wuss__close_hit_box(const wuss_window_t *window, box_t *out)
{
  box_t drawn;

  wuss__close_box(window, &drawn);

  out->x0 = (window->flags & wuss_WINDOW_BACK) ? drawn.x0 : window->visible.x0;
  out->y0 = window->visible.y0;
  out->x1 = drawn.x1;
  out->y1 = drawn.y1;
}
