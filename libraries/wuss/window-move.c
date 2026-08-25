/* window-move.c -- wuss - minimal window manager */

#include "impl.h"

/* x,y is the window's content top-left; the furniture offset (outline plus
 * any titlebar) is constant for a given window, so the footprint just
 * follows it */
void wuss_window_move(wuss_window_t *window, int x, int y)
{
  int   width, height, outline_px, titlebar_height;
  box_t before, dirty;

  width           = window->visible.x1 - window->visible.x0;
  height          = window->visible.y1 - window->visible.y0;
  outline_px      = wuss__outline_px(window);
  titlebar_height = wuss__titlebar_height(window);
  before          = window->visible;

  window->visible.x0 = x - outline_px;
  window->visible.y0 = y - outline_px - titlebar_height;
  window->visible.x1 = window->visible.x0 + width;
  window->visible.y1 = window->visible.y0 + height;

  if (window->wuss->z_order.next == &window->link &&
      screen_copy_rect(window->wuss->scr, &before, window->visible.x0, window->visible.y0))
  {
    /* Topmost, and the screen format supports the blit: every pixel of
     * "before" is genuinely this window's own rendering (nothing above it
     * to have punched holes in it), so sliding those pixels to the new
     * position is exactly as correct as asking the client to redraw there,
     * but far cheaper -- only the vacated sliver behind the old position
     * still needs an actual repaint. */
    wuss__invalidate_minus(window->wuss, &before, &window->visible);
  }
  else
  {
    /* Not topmost, or the blit was declined (e.g. paletted screen): fall
     * back to a normal clipped redraw of the whole moved footprint. */
    box_union(&before, &window->visible, &dirty);
    wuss__invalidate_clipped(window, &dirty);
  }
}
