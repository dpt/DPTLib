/* invalidate.c -- wuss - minimal window manager */

#include "../impl.h"

/* Same as wuss__furniture_invalidate, but against an arbitrary box rather
 * than window->visible -- lets a caller that's just resized the window
 * (toggle-size) also mark the *old* furniture strips dirty, since a blit
 * that reused the old pixels leaves stale titlebar/scrollbar/outline
 * pixels sitting wherever those strips used to be. */
void wuss__furniture_invalidate_for(wuss_window_t *window, const box_t *visible)
{
  int     outline_px;
  point_t carve;

  outline_px = wuss__outline_px(window);
  /* Ask for the same carve the layout uses rather than reading the
   * scrollbar flags directly: a window with both scrollbars off but resize
   * on still reserves both strips, for the resize icon and the rules. */
  wuss__furniture_carve_for(window->flags, wuss__button_size(window), &carve);

  if (!(window->flags & wuss_WINDOW_NO_TITLEBAR))
  {
    box_t titlebar;

    titlebar.x0 = visible->x0 + outline_px;
    titlebar.y0 = visible->y0 + outline_px;
    titlebar.x1 = visible->x1 - outline_px;
    titlebar.y1 = titlebar.y0 + wuss__titlebar_height(window);
    wuss__invalidate_clipped(window, &titlebar);
  }

  if (carve.x > 0)
  {
    box_t column;

    column.x1 = visible->x1 - outline_px;
    column.x0 = column.x1 - carve.x; /* includes the interior rule */
    column.y0 = visible->y0;
    column.y1 = visible->y1;
    wuss__invalidate_clipped(window, &column);
  }

  if (carve.y > 0)
  {
    box_t row;

    row.y1 = visible->y1 - outline_px;
    row.y0 = row.y1 - carve.y; /* includes the interior rule */
    row.x0 = visible->x0;
    row.x1 = visible->x1;
    wuss__invalidate_clipped(window, &row);
  }

  if (outline_px > 0)
  {
    box_t edge;

    edge = *visible;
    edge.y1 = edge.y0 + outline_px;
    wuss__invalidate_clipped(window, &edge); /* top */

    edge = *visible;
    edge.y0 = edge.y1 - outline_px;
    wuss__invalidate_clipped(window, &edge); /* bottom */

    edge = *visible;
    edge.x1 = edge.x0 + outline_px;
    wuss__invalidate_clipped(window, &edge); /* left */

    edge = *visible;
    edge.x0 = edge.x1 - outline_px;
    wuss__invalidate_clipped(window, &edge); /* right */
  }
}

void wuss__furniture_invalidate(wuss_window_t *window)
{
  wuss__furniture_invalidate_for(window, &window->visible);
}
