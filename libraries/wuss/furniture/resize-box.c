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

/* The resize hit box owns the bottom-right corner: it grows right and down over
 * the outline. On a side with no scroll strip it also grows inward to the
 * content edge, swallowing the divider seam (scrollbars present) or the whole
 * carved margin (NO_VSCROLL + NO_HSCROLL + resize on). On a side that does have
 * a scroll strip its edge is unchanged and butts that strip. */
void wuss__resize_hit_box(const wuss_window_t *window, box_t *out)
{
  point_t carve;
  int     outline_px, size;

  outline_px = wuss__outline_px(window);
  size       = wuss__button_size(window);
  wuss__furniture_carve_for(window->flags, size, &carve);

  out->x1 = window->visible.x1;
  out->y1 = window->visible.y1;

  if (!(window->flags & wuss_WINDOW_NO_VSCROLL))
    out->x0 = window->visible.x1 - outline_px - size;   /* butt the vscroll strip */
  else
    out->x0 = window->visible.x1 - outline_px - carve.x; /* = content right edge */

  if (!(window->flags & wuss_WINDOW_NO_HSCROLL))
    out->y0 = window->visible.y1 - outline_px - size;   /* butt the hscroll strip */
  else
    out->y0 = window->visible.y1 - outline_px - carve.y; /* = content bottom edge */
}
