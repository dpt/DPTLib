/* scroll.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_scroll(wuss_t *wuss, int x, int y, int delta, wuss_window_t **hit)
{
  wuss_window_t *win;

  win = wuss__window_at(wuss, x, y);
  if (hit != NULL)
    *hit = win;

  if (win == NULL)
    return result_OK;

  if (win->task.scroll != NULL)
  {
    box_t titlebar, content;
    int   local_x, local_y;

    wuss__titlebar_box(win, &titlebar);
    if (box_contains_point(&titlebar, x, y))
      return result_OK;

    wuss__content_box(win, &content);
    local_x = x - content.x0;
    local_y = y - content.y0;
    return win->task.scroll(win, local_x, local_y, delta, win->task.task_data);
  }

  return result_OK;
}
