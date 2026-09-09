/* wuss/furniture/resize-box.c -- wuss - minimal window manager */

#include "../core/impl.h"

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

/* The resize hit box owns the bottom-right corner: the button-sized drawn
 * icon square, grown out over the outline band so the extreme corner still
 * hits. Independent of which scrollbars are present -- a strip butts the
 * icon at the same edge the icon would sit at with no strip there. */
void wuss__resize_hit_box(const wuss_window_t *window, box_t *out)
{
  int outline_px, size;

  outline_px = wuss__outline_px(window);
  size       = wuss__button_size(window);

  /* The drawn icon (wuss__resize_box) is always the button-sized square at
   * the bottom-right, inside the outline. Match it exactly, then grow out
   * over the outline band so the very corner still counts as a hit. Whether
   * a scroll strip butts the icon or (no strip) nothing paints that margin,
   * the icon's near edges are the same, so no per-axis branch is needed. */
  out->x0 = window->visible.x1 - outline_px - size;
  out->y0 = window->visible.y1 - outline_px - size;
  out->x1 = window->visible.x1;
  out->y1 = window->visible.y1;
}
