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

  box_union(&before, &window->visible, &dirty);
  wuss__invalidate_clipped(window, &dirty);
}
