/* invalidate.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss__furniture_invalidate(wuss_window_t *window)
{
  box_t titlebar;
  int   outline_px;

  wuss__titlebar_box(window, &titlebar);
  wuss__invalidate_clipped(window, &titlebar);

  outline_px = wuss__outline_px(window);
  if (outline_px > 0)
  {
    box_t edge;

    edge = window->visible;
    edge.y1 = edge.y0 + outline_px;
    wuss__invalidate_clipped(window, &edge); /* top */

    edge = window->visible;
    edge.y0 = edge.y1 - outline_px;
    wuss__invalidate_clipped(window, &edge); /* bottom */

    edge = window->visible;
    edge.x1 = edge.x0 + outline_px;
    wuss__invalidate_clipped(window, &edge); /* left */

    edge = window->visible;
    edge.x0 = edge.x1 - outline_px;
    wuss__invalidate_clipped(window, &edge); /* right */
  }
}
