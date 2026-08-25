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

  if (win->task.handle != NULL)
  {
    box_t        titlebar, content;
    wuss_event_t event;

    wuss__titlebar_box(win, &titlebar);
    if (box_contains_point(&titlebar, x, y))
      return result_OK;

    wuss__content_box(win, &content);
    event.kind             = wuss_EVENT_MOUSE;
    event.data.mouse.action = wuss_MOUSE_MOVE;
    event.data.mouse.x      = x - content.x0 + win->scroll_x;
    event.data.mouse.y      = y - content.y0 + win->scroll_y;
    event.data.mouse.button = wuss_BUTTON_SELECT;
    return win->task.handle(win, &event, win->task.task_data);
  }

  return result_OK;
}
