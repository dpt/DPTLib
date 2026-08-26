/* scroll.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_scroll(wuss_t *wuss, point_t p, int delta, wuss_window_t **hit)
{
  wuss_window_t *win;
  box_t          titlebar;
  int            x, y;

  x = p.x;
  y = p.y;

  win = wuss__window_at(wuss, p);
  if (hit != NULL)
    *hit = win;

  if (win == NULL)
    return result_OK;

  wuss__titlebar_box(win, &titlebar);
  if (box_contains_point(&titlebar, x, y))
    return result_OK;

  wuss__furniture_scroll_step(win, (point_t) { 0, delta });

  if (win->task.handle != NULL)
  {
    box_t        content;
    wuss_event_t event;

    wuss__content_box(win, &content);
    event.kind                = wuss_EVENT_SCROLL;
    event.data.scroll.point.x = x - content.x0 + win->scroll.x;
    event.data.scroll.point.y = y - content.y0 + win->scroll.y;
    event.data.scroll.delta   = delta;
    return win->task.handle(win, &event, win->task.task_data);
  }

  return result_OK;
}
