/* mouse-click.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_mouse_click(wuss_t              *wuss,
                          int                  x,
                          int                  y,
                          wuss_button_t        button,
                          wuss_mouse_action_t  action,
                          wuss_window_t      **hit)
{
  wuss_window_t          *win;
  wuss_furniture_region_t region;
  wuss_event_t            event;

  if (action == wuss_MOUSE_UP && wuss->dragging != NULL)
  {
    win = wuss->dragging;
    if (hit != NULL)
      *hit = win;
    wuss->dragging = NULL;

    /* an Adjust click on a titlebar that never moved is a click, not a
     * drag: send the window to the back instead */
    if (button == wuss_BUTTON_ADJUST && !wuss->drag_moved)
      wuss_window_restack(win, wuss_ZORDER_BACK);

    return result_OK;
  }

  win = wuss__window_at(wuss, x, y);
  if (hit != NULL)
    *hit = win;

  if (win == NULL)
    return result_OK;

  region = wuss__furniture_hit_test(win, x, y);

  if (region == wuss_FURNITURE_CLOSE &&
      action == wuss_MOUSE_DOWN     &&
      button == wuss_BUTTON_SELECT)
  {
    if (win->task.handle == NULL)
      return result_OK;

    event.kind = wuss_EVENT_CLOSE;
    return win->task.handle(win, &event, win->task.task_data);
  }

  if (region == wuss_FURNITURE_CLOSE || region == wuss_FURNITURE_TITLE)
  {
    if (action == wuss_MOUSE_DOWN)
    {
      box_t content;

      if (button == wuss_BUTTON_SELECT)
        wuss_window_restack(win, wuss_ZORDER_FRONT);

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
