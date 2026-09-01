/* wuss/scroll-step.c -- clamp and apply a scroll offset */

#include "impl.h"

point_t wuss__scroll_clamp(const wuss_window_t *window, point_t desired)
{
  box_t content;
  int   max_x, max_y;

  wuss__content_box(window, &content);

  max_x = window->doc.w  - (content.x1 - content.x0);
  max_y = window->doc.h - (content.y1 - content.y0);
  if (max_x < 0)
    max_x = 0;
  if (max_y < 0)
    max_y = 0;

  if (desired.x < 0)
    desired.x = 0;
  else if (desired.x > max_x)
    desired.x = max_x;
  if (desired.y < 0)
    desired.y = 0;
  else if (desired.y > max_y)
    desired.y = max_y;

  return desired;
}

void wuss__scroll_step(wuss_window_t *window, point_t delta)
{
  point_t scroll;

  /* A window that declared an axis non-scrollable (e.g. a pop-up menu) has no
   * scrollbar to clamp against and its geometry assumes scroll == 0, so a wheel
   * turn over it must not move the content -- doing so corrupts the display. */
  if (window->flags & wuss_WINDOW_NO_HSCROLL)
    delta.x = 0;
  if (window->flags & wuss_WINDOW_NO_VSCROLL)
    delta.y = 0;
  if (delta.x == 0 && delta.y == 0)
    return;

  wuss_window_get_scroll(window, &scroll);
  scroll.x += delta.x;
  scroll.y += delta.y;
  scroll = wuss__scroll_clamp(window, scroll);

  wuss_window_set_scroll(window, scroll);
}
