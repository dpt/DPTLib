/* content-box.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss__content_box(const wuss_window_t *window, box_t *out)
{
  int outline_px, size;

  outline_px = wuss__outline_px(window);

  out->x0 = window->visible.x0 + outline_px;
  out->y0 = window->visible.y0 + outline_px + wuss__titlebar_height(window);
  out->x1 = window->visible.x1 - outline_px;
  out->y1 = window->visible.y1 - outline_px;

  size = wuss__icon_size(window);
  if (!(window->flags & wuss_WINDOW_NO_VSCROLL))
    out->x1 -= size;
  if (!(window->flags & wuss_WINDOW_NO_HSCROLL))
    out->y1 -= size;

  /* the resize icon always occupies the bottom-right corner (see
   * wuss__resize_box); if neither scrollbar's own reserved band already
   * covers that corner, carve it out here */
  if (!(window->flags & wuss_WINDOW_NO_RESIZE) &&
      (window->flags & wuss_WINDOW_NO_VSCROLL) &&
      (window->flags & wuss_WINDOW_NO_HSCROLL))
  {
    out->x1 -= size;
    out->y1 -= size;
  }
}
