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

    switch (wuss->drag_kind)
    {
    case wuss_FURNITURE_DRAG_RESIZE:
      wuss__furniture_drag_resize(win, (point_t) { x, y });
      break;

    case wuss_FURNITURE_DRAG_VSCROLL_THUMB:
      wuss__furniture_drag_thumb(win, y - wuss->drag.y, wuss->drag_scroll_start, 0);
      break;

    case wuss_FURNITURE_DRAG_HSCROLL_THUMB:
      wuss__furniture_drag_thumb(win, x - wuss->drag.x, wuss->drag_scroll_start, 1);
      break;

    case wuss_FURNITURE_DRAG_MOVE:
    default:
      wuss_window_move(win, x - wuss->drag.x, y - wuss->drag.y);
      break;
    }

    return result_OK;
  }

  win = wuss__window_at(wuss, x, y);
  if (hit != NULL)
    *hit = win;

  if (win == NULL)
    return result_OK;

  if (wuss__furniture_hit_test(win, (point_t) { x, y }) != wuss_FURNITURE_CONTENT)
    return result_OK;

  if (win->task.handle != NULL)
  {
    box_t        content;
    wuss_event_t event;

    wuss__content_box(win, &content);
    event.kind               = wuss_EVENT_MOUSE;
    event.data.mouse.action  = wuss_MOUSE_MOVE;
    event.data.mouse.point.x = x - content.x0 + win->scroll.x;
    event.data.mouse.point.y = y - content.y0 + win->scroll.y;
    event.data.mouse.button  = wuss_BUTTON_SELECT;
    return win->task.handle(win, &event, win->task.task_data);
  }

  return result_OK;
}
