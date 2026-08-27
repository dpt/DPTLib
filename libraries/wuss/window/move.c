/* move.c -- wuss - minimal window manager */

#include "../impl.h"

/* true if any window above "window" in z-order overlaps "before" -- if
 * so, "before" isn't purely this window's own rendering and the blit
 * fast path in wuss_window_move() must not be used */
static int wuss__occluded_above(wuss_window_t *window, const box_t *before)
{
  list_t *e;

  for (e = window->wuss->z_order.next; e != &window->link; e = e->next)
  {
    if (box_intersects(&((wuss_window_t *) e)->visible, before))
      return 1;
  }

  return 0;
}

/* p is the window's content top-left; the furniture offset (outline plus
 * any titlebar) is constant for a given window, so the footprint just
 * follows it */
void wuss_window_move(wuss_window_t *window, point_t p)
{
  int   width, height, outline_px, titlebar_height;
  box_t before, dirty, copied;

  width           = window->visible.x1 - window->visible.x0;
  height          = window->visible.y1 - window->visible.y0;
  outline_px      = wuss__outline_px(window);
  titlebar_height = wuss__titlebar_height(window);
  before          = window->visible;

  window->visible.x0 = p.x - outline_px;
  window->visible.y0 = p.y - outline_px - titlebar_height;
  window->visible.x1 = window->visible.x0 + width;
  window->visible.y1 = window->visible.y0 + height;

  wuss__notify_open(window);

  if (!wuss__occluded_above(window, &before) &&
      screen_copy_rect(window->wuss->scr, &before,
                       (point_t) { window->visible.x0, window->visible.y0 }, &copied))
  {
    /* Nothing above this window overlaps its old footprint, and the screen
     * format supports the blit: every pixel of "before" is genuinely this
     * window's own rendering (nothing above it to have punched holes in
     * it), so sliding those pixels to the new position is exactly as
     * correct as asking the task to redraw there, but far cheaper -- only
     * the vacated sliver behind the old position still needs an actual
     * repaint. */
    wuss__invalidate_minus(window->wuss, &before, &window->visible);

    /* "copied" can be smaller than the new footprint if either end of the
     * move was partly off-screen (e.g. dragging back on-screen from
     * off-screen): the leftover part has no valid source pixels behind it,
     * so it needs a real repaint too, not just the vacated sliver above. */
    wuss__invalidate_minus(window->wuss, &window->visible, &copied);
  }
  else
  {
    /* Something above overlaps the old footprint, or the blit was declined
     * (e.g. paletted screen): fall back to a normal clipped redraw of the
     * whole moved footprint. */
    box_union(&before, &window->visible, &dirty);
    wuss__invalidate_clipped(window, &dirty);
  }
}
