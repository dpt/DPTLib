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
    wuss->dragging  = NULL;
    wuss->drag_kind = wuss_FURNITURE_DRAG_NONE;

    return result_OK;
  }

  win = wuss__window_at(wuss, x, y);
  if (hit != NULL)
    *hit = win;

  if (win == NULL)
    return result_OK;

  region = wuss__furniture_hit_test(win, (point_t) { x, y });

  if (region == wuss_FURNITURE_CLOSE &&
      action == wuss_MOUSE_DOWN     &&
      button == wuss_BUTTON_SELECT)
  {
    if (win->task.handle == NULL)
      return result_OK;

    event.kind = wuss_EVENT_CLOSE;
    return win->task.handle(win, &event, win->task.task_data);
  }

  if (region == wuss_FURNITURE_BACK && action == wuss_MOUSE_DOWN)
  {
    if (button == wuss_BUTTON_SELECT)
      wuss_window_restack(win, wuss_ZORDER_BACK);
    else if (button == wuss_BUTTON_ADJUST)
      wuss_window_restack(win, wuss_ZORDER_FRONT);
    return result_OK;
  }

  if (region == wuss_FURNITURE_TOGGLE_SIZE ||
      region == wuss_FURNITURE_VSCROLL_UP  ||
      region == wuss_FURNITURE_VSCROLL_DOWN ||
      region == wuss_FURNITURE_HSCROLL_LEFT ||
      region == wuss_FURNITURE_HSCROLL_RIGHT)
  {
    if (action == wuss_MOUSE_DOWN && button == wuss_BUTTON_SELECT)
    {
      switch (region)
      {
      case wuss_FURNITURE_TOGGLE_SIZE:
        wuss__furniture_toggle_size(win);
        break;
      case wuss_FURNITURE_VSCROLL_UP:
        wuss__furniture_scroll_step(win, (point_t) { 0, -WUSS_SCROLL_STEP });
        break;
      case wuss_FURNITURE_VSCROLL_DOWN:
        wuss__furniture_scroll_step(win, (point_t) { 0, WUSS_SCROLL_STEP });
        break;
      case wuss_FURNITURE_HSCROLL_LEFT:
        wuss__furniture_scroll_step(win, (point_t) { -WUSS_SCROLL_STEP, 0 });
        break;
      case wuss_FURNITURE_HSCROLL_RIGHT:
        wuss__furniture_scroll_step(win, (point_t) { WUSS_SCROLL_STEP, 0 });
        break;
      default:
        break;
      }
    }
    return result_OK;
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
      wuss->drag_kind  = wuss_FURNITURE_DRAG_MOVE;
      wuss->drag.x     = x - content.x0;
      wuss->drag.y     = y - content.y0;
    }
    return result_OK;
  }

  if (region == wuss_FURNITURE_RESIZE ||
      region == wuss_FURNITURE_VSCROLL_BAR ||
      region == wuss_FURNITURE_HSCROLL_BAR)
  {
    if (action == wuss_MOUSE_DOWN)
    {
      int sx, sy;

      if (button == wuss_BUTTON_SELECT)
        wuss_window_restack(win, wuss_ZORDER_FRONT);

      wuss_window_get_scroll(win, &sx, &sy);

      wuss->dragging          = win;
      wuss->drag_kind         = wuss__furniture_drag_kind(region);
      wuss->drag.x            = x;
      wuss->drag.y            = y;
      wuss->drag_scroll_start = (region == wuss_FURNITURE_VSCROLL_BAR) ? sy : sx;
    }
    return result_OK;
  }

  if (win->task.handle != NULL)
  {
    box_t content;

    wuss__content_box(win, &content);
    event.kind               = wuss_EVENT_MOUSE;
    event.data.mouse.action  = action;
    event.data.mouse.point.x = x - content.x0 + win->scroll.x;
    event.data.mouse.point.y = y - content.y0 + win->scroll.y;
    event.data.mouse.button  = button;
    return win->task.handle(win, &event, win->task.task_data);
  }

  return result_OK;
}
