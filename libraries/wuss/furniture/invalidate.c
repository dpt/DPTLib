/* invalidate.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss__furniture_invalidate(wuss_window_t *window)
{
  box_t titlebar;
  int   outline_px;

  if (!(window->flags & wuss_WINDOW_NO_TITLEBAR))
  {
    wuss__titlebar_box(window, &titlebar);
    wuss__invalidate_clipped(window, &titlebar);
  }

  if (!(window->flags & wuss_WINDOW_NO_VSCROLL))
  {
    box_t column;

    column.x1 = window->visible.x1 - wuss__outline_px(window);
    column.x0 = column.x1 - wuss__icon_size(window);
    column.y0 = window->visible.y0;
    column.y1 = window->visible.y1;
    wuss__invalidate_clipped(window, &column);
  }

  if (!(window->flags & wuss_WINDOW_NO_HSCROLL))
  {
    box_t row;

    row.y1 = window->visible.y1 - wuss__outline_px(window);
    row.y0 = row.y1 - wuss__icon_size(window);
    row.x0 = window->visible.x0;
    row.x1 = window->visible.x1;
    wuss__invalidate_clipped(window, &row);
  }

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
