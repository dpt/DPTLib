/* wuss/furniture/titlebar-box.c -- wuss - minimal window manager */

#include "../core/impl.h"

void wuss__titlebar_box(const wuss_window_t *window, box_t *out)
{
  int outline_px;

  outline_px = wuss__outline_px(window);

  out->x0 = window->visible.x0 + outline_px;
  out->y0 = window->visible.y0 + outline_px;
  out->x1 = window->visible.x1 - outline_px;
  out->y1 = out->y0 + wuss__titlebar_height(window);
}

/* The titlebar's hit box grows over the top, left and right outline so a click
 * on the outline band there routes to TITLE rather than falling through to the
 * workarea. The bottom edge is unchanged -- it butts the content top, which
 * does not move. */
void wuss__title_hit_box(const wuss_window_t *window, box_t *out)
{
  int outline_px;

  outline_px = wuss__outline_px(window);

  out->x0 = window->visible.x0;
  out->y0 = window->visible.y0;
  out->x1 = window->visible.x1;
  out->y1 = window->visible.y0 + outline_px + wuss__titlebar_height(window);
}
