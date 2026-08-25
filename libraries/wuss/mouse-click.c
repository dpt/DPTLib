/* mouse-click.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_mouse_click(wuss_t              *wuss,
                          int                  x,
                          int                  y,
                          wuss_button_t        button,
                          wuss_mouse_action_t  action,
                          wuss_window_t      **hit)
{
  wuss_window_t *win;
  box_t          titlebar;
  wuss_event_t   event;

  if (action == wuss_MOUSE_UP && wuss->dragging != NULL)
  {
    win = wuss->dragging;
    if (hit != NULL)
      *hit = win;
    wuss->dragging = NULL;

    /* an Adjust click on a titlebar that never moved is a click, not a
     * drag: send the window to the back instead */
    if (button == wuss_BUTTON_ADJUST && !wuss->drag_moved)
      wuss_window_send_to_back(win);

    return result_OK;
  }

  win = wuss__window_at(wuss, x, y);
  if (hit != NULL)
    *hit = win;

  if (win == NULL)
    return result_OK;

  if (!(win->flags & wuss_WINDOW_NO_TITLEBAR) &&
      !(win->flags & wuss_WINDOW_NO_CLOSE)    &&
      action == wuss_MOUSE_DOWN               &&
      button == wuss_BUTTON_SELECT)
  {
    box_t close;

    wuss__close_box(win, &close);
    if (box_contains_point(&close, x, y))
    {
      if (win->task.handle == NULL)
        return result_OK;

      event.kind = wuss_EVENT_CLOSE;
      return win->task.handle(win, &event, win->task.task_data);
    }
  }

  wuss__titlebar_box(win, &titlebar);
  if (box_contains_point(&titlebar, x, y))
  {
    if (action == wuss_MOUSE_DOWN)
    {
      box_t content;

      if (button == wuss_BUTTON_SELECT)
        wuss_window_bring_to_front(win);

      wuss__content_box(win, &content);
      wuss->dragging   = win;
      wuss->drag_dx    = x - content.x0;
      wuss->drag_dy    = y - content.y0;
      wuss->drag_moved = 0;
    }
    return result_OK;
  }

  if (win->task.handle != NULL)
  {
    box_t content;

    wuss__content_box(win, &content);
    event.kind             = wuss_EVENT_MOUSE;
    event.data.mouse.action = action;
    event.data.mouse.x      = x - content.x0 + win->scroll_x;
    event.data.mouse.y      = y - content.y0 + win->scroll_y;
    event.data.mouse.button = button;
    return win->task.handle(win, &event, win->task.task_data);
  }

  return result_OK;
}
