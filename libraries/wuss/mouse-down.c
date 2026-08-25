/* mouse-down.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_mouse_down(wuss_t *wuss, int x, int y, wuss_button_t button, wuss_window_t **hit)
{
  wuss_window_t *win;
  box_t          titlebar;

  win = wuss__window_at(wuss, x, y);
  if (hit != NULL)
    *hit = win;

  if (win == NULL)
    return result_OK;

  wuss__titlebar_box(win, &titlebar);
  if (box_contains_point(&titlebar, x, y))
  {
    box_t content;

    if (button == wuss_BUTTON_SELECT)
      wuss_window_bring_to_front(win);

    wuss__content_box(win, &content);
    wuss->dragging   = win;
    wuss->drag_dx    = x - content.x0;
    wuss->drag_dy    = y - content.y0;
    wuss->drag_moved = 0;
    return result_OK;
  }

  if (win->task.mouse != NULL)
  {
    box_t content;
    int   local_x, local_y;

    wuss__content_box(win, &content);
    local_x = x - content.x0;
    local_y = y - content.y0;
    return win->task.mouse(win, wuss_MOUSE_DOWN, local_x, local_y, button, win->task.task_data);
  }

  return result_OK;
}
