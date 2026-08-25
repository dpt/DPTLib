/* mouse-move.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_mouse_move(wuss_t *wuss, int x, int y, wuss_window_t **hit)
{
  wuss_window_t *win;

  if (wuss->dragging != NULL)
  {
    win = wuss->dragging;
    if (hit != NULL)
      *hit = win;
    wuss->drag_moved = 1;
    wuss_window_move(win, x - wuss->drag_dx, y - wuss->drag_dy);
    return result_OK;
  }

  win = wuss__window_at(wuss, x, y);
  if (hit != NULL)
    *hit = win;

  if (win == NULL)
    return result_OK;

  if (win->task.mouse != NULL)
  {
    box_t titlebar, content;
    int   local_x, local_y;

    wuss__titlebar_box(win, &titlebar);
    if (box_contains_point(&titlebar, x, y))
      return result_OK;

    wuss__content_box(win, &content);
    local_x = x - content.x0;
    local_y = y - content.y0;
    return win->task.mouse(win, wuss_MOUSE_MOVE, local_x, local_y, wuss_BUTTON_SELECT, win->task.task_data);
  }

  return result_OK;
}
