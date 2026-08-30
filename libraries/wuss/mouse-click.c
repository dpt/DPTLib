/* mouse-click.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_mouse_click(wuss_t             *wuss,
                          point_t             p,
                          wuss_button_t       button,
                          wuss_mouse_action_t action,
                          wuss_window_t     **hit)
{
  wuss_window_t          *win;
  wuss_furniture_region_t region;
  wuss_event_t            event;
  int                     x, y;

  x = p.x;
  y = p.y;

  /* Release a held button icon on any MOUSE_UP, before the hit-test picks a
   * window: the up may land on a window that opened over the icon's owner on
   * MOUSE_DOWN, so wuss__window_at would never reach the pressed icon. */
  if (action == wuss_MOUSE_UP && wuss->pressed_icon != NULL)
  {
    wuss_icon_t *pressed = wuss->pressed_icon;

    wuss->pressed_icon = NULL;
    if (pressed->pressed)
    {
      pressed->pressed = 0;
      wuss__icon_invalidate(pressed);
    }
  }

  if (action == wuss_MOUSE_UP && wuss->furniture.dragging != NULL)
  {
    win = wuss->furniture.dragging;
    if (hit != NULL)
      *hit = win;
    wuss->furniture.dragging  = NULL;
    wuss->furniture.drag_kind = wuss_FURNITURE_DRAG_NONE;

    return result_OK;
  }

  win = wuss__window_at(wuss, p);
  if (hit != NULL)
    *hit = win;

  if (win == NULL)
    return result_OK;

  region = wuss__furniture_hit_test(win, POINT(x, y));

  if (region == wuss_FURNITURE_CLOSE  &&
      action == wuss_MOUSE_DOWN       &&
      (button & wuss_BUTTON_SELECT))
  {
    if (win->task.handle == NULL)
      return result_OK;

    event.kind = wuss_EVENT_CLOSE;
    return win->task.handle(win, &event, win->task.task_data);
  }

  if (region == wuss_FURNITURE_BACK && action == wuss_MOUSE_DOWN)
  {
    if (button & wuss_BUTTON_SELECT)
      wuss_window_restack(win, wuss_ZORDER_BACK);
    else if (button & wuss_BUTTON_ADJUST)
      wuss_window_restack(win, wuss_ZORDER_FRONT);
    return result_OK;
  }

  if (region == wuss_FURNITURE_TOGGLE_SIZE ||
      region == wuss_FURNITURE_VSCROLL_UP  ||
      region == wuss_FURNITURE_VSCROLL_DOWN ||
      region == wuss_FURNITURE_HSCROLL_LEFT ||
      region == wuss_FURNITURE_HSCROLL_RIGHT)
  {
    if (action == wuss_MOUSE_DOWN &&
        (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
    {
      /* Adjust-clicking a scroll arrow steps the opposite way to the arrow it
       * points, so one arrow can be worked in both directions without moving
       * the pointer. Toggle-size stays Select-only. */
      int step;

      /* Select wins a Select+Adjust chord, so a chord never scrolls backwards
       * unexpectedly. */
      step = (button & wuss_BUTTON_SELECT) ?  WUSS_SCROLL_STEP
                                           : -WUSS_SCROLL_STEP;

      switch (region)
      {
      case wuss_FURNITURE_TOGGLE_SIZE:
        if (button & wuss_BUTTON_SELECT)
          wuss__furniture_toggle_size(win);
        break;
      case wuss_FURNITURE_VSCROLL_UP:
        wuss__furniture_scroll_step(win, POINT(0, -step));
        break;
      case wuss_FURNITURE_VSCROLL_DOWN:
        wuss__furniture_scroll_step(win, POINT(0, step));
        break;
      case wuss_FURNITURE_HSCROLL_LEFT:
        wuss__furniture_scroll_step(win, POINT(-step, 0));
        break;
      case wuss_FURNITURE_HSCROLL_RIGHT:
        wuss__furniture_scroll_step(win, POINT(step, 0));
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

      if (button & wuss_BUTTON_SELECT)
        wuss_window_restack(win, wuss_ZORDER_FRONT);

      wuss__content_box(win, &content);
      wuss->furniture.dragging   = win;
      wuss->furniture.drag_kind  = wuss_FURNITURE_DRAG_MOVE;
      wuss->furniture.drag.x     = x - content.x0;
      wuss->furniture.drag.y     = y - content.y0;
    }
    return result_OK;
  }

  if (region == wuss_FURNITURE_RESIZE ||
      region == wuss_FURNITURE_VSCROLL_WELL ||
      region == wuss_FURNITURE_HSCROLL_WELL)
  {
    if (action == wuss_MOUSE_DOWN)
    {
      point_t scroll;

      /* Only resize raises the window; dragging a scrollbar well must not
       * reorder the stack. */
      if ((button & wuss_BUTTON_SELECT) && region == wuss_FURNITURE_RESIZE)
        wuss_window_restack(win, wuss_ZORDER_FRONT);

      wuss_window_get_scroll(win, &scroll);

      wuss->furniture.dragging          = win;
      wuss->furniture.drag_kind         = wuss__furniture_drag_kind(region);
      wuss->furniture.drag.x            = x;
      wuss->furniture.drag.y            = y;
      wuss->furniture.drag_scroll_start = (region == wuss_FURNITURE_VSCROLL_WELL) ? scroll.y : scroll.x;

      /* Resize needs the pointer's offset from the content box's current
       * bottom-right corner, so the point grabbed on the resize icon stays
       * under the pointer as it moves, rather than that corner jumping to
       * meet the pointer on the very first move. */
      if (region == wuss_FURNITURE_RESIZE)
      {
        box_t content;
        wuss__content_box(win, &content);
        wuss->furniture.drag_offset.x = x - content.x1;
        wuss->furniture.drag_offset.y = y - content.y1;
      }
    }
    return result_OK;
  }

  if (win->task.handle != NULL)
  {
    box_t        content;
    point_t      doc_point;
    wuss_icon_t *icon;

    wuss__content_box(win, &content);
    doc_point.x = x - content.x0 + win->scroll.x;
    doc_point.y = y - content.y0 + win->scroll.y;

    icon = wuss__icon_hit_test(win, doc_point);
    if (icon != NULL)
    {
      if (action == wuss_MOUSE_DOWN &&
          (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
      {
        icon->pressed      = 1;
        wuss->pressed_icon = icon;
        wuss__icon_invalidate(icon);
      }
      else if (action == wuss_MOUSE_UP && icon->pressed)
      {
        icon->pressed      = 0;
        wuss->pressed_icon = NULL;
        wuss__icon_invalidate(icon);
      }

      event.kind             = wuss_EVENT_ICON;
      event.data.icon.icon   = icon;
      event.data.icon.action = action;
      event.data.icon.button = button;
      return win->task.handle(win, &event, win->task.task_data);
    }

    event.kind              = wuss_EVENT_MOUSE;
    event.data.mouse.action = action;
    event.data.mouse.point  = doc_point;
    event.data.mouse.button = button;
    return win->task.handle(win, &event, win->task.task_data);
  }

  return result_OK;
}
