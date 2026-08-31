/* mouse-move.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_mouse_move(wuss_t *wuss, point_t p, wuss_window_t **hit)
{
  wuss_window_t *win;
  int            x, y;

  x = p.x;
  y = p.y;

  wuss->pointer = p;

#ifdef WUSS_FURNITURE
  if (wuss->furniture.dragging != NULL)
  {
    win = wuss->furniture.dragging;
    if (hit != NULL)
      *hit = win;

    switch (wuss->furniture.drag_kind)
    {
    case wuss_FURNITURE_DRAG_RESIZE:
      wuss__furniture_drag_resize(win, POINT(x, y));
      break;

    case wuss_FURNITURE_DRAG_VSCROLL_SAUSAGE:
      wuss__furniture_drag_sausage(win, y - wuss->furniture.drag.y, wuss->furniture.drag_scroll_start, 0);
      break;

    case wuss_FURNITURE_DRAG_HSCROLL_SAUSAGE:
      wuss__furniture_drag_sausage(win, x - wuss->furniture.drag.x, wuss->furniture.drag_scroll_start, 1);
      break;

    case wuss_FURNITURE_DRAG_MOVE:
    default:
      wuss_window_move(win, POINT(x - wuss->furniture.drag.x, y - wuss->furniture.drag.y));
      break;
    }

    return result_OK;
  }
#endif

  win = wuss__window_at(wuss, p);
  if (hit != NULL)
    *hit = win;

  if (win == NULL)
  {
#ifdef WUSS_ICONS
    wuss__icon_set_hover(wuss, NULL);
#endif
    return result_OK;
  }

#ifdef WUSS_FURNITURE
  if (wuss__furniture_hit_test(win, POINT(x, y)) != wuss_FURNITURE_CONTENT)
  {
#ifdef WUSS_ICONS
    wuss__icon_set_hover(wuss, NULL);
#endif
    return result_OK;
  }
#endif

  if (win->task.handle == NULL)
  {
#ifdef WUSS_ICONS
    wuss__icon_set_hover(wuss, NULL);
#endif
    return result_OK;
  }

  {
    box_t        content;
    point_t      doc_point;
    wuss_event_t event;

    wuss__content_box(win, &content);
    doc_point.x = x - content.x0 + win->scroll.x;
    doc_point.y = y - content.y0 + win->scroll.y;

#ifdef WUSS_ICONS
    {
      wuss_icon_t *icon;
      int          k;

      icon = wuss__icon_hit_test(win, doc_point);

      wuss__icon_set_hover(wuss, icon);

      /* Clear the pressed state of any button the pointer has left. This does
       * not re-press a button on drag-back-in, and does not track which mouse
       * button is held -- wuss keeps no persistent "button down over content"
       * state. */
      for (k = 0; k < win->nicons; k++)
      {
        wuss_icon_t *it = win->icons[k];

        if (it->pressed && it != icon)
        {
          it->pressed = 0;
          if (wuss->pressed_icon == it)
            wuss->pressed_icon = NULL;
          wuss__icon_invalidate(it);
        }
      }

      if (icon != NULL)
      {
        event.kind             = wuss_EVENT_ICON;
        event.data.icon.icon   = icon;
        event.data.icon.action = wuss_MOUSE_MOVE;
        event.data.icon.button = wuss_BUTTON_SELECT;
        return win->task.handle(win, &event, win->task.task_data);
      }
    }
#endif

    event.kind              = wuss_EVENT_MOUSE;
    event.data.mouse.action = wuss_MOUSE_MOVE;
    event.data.mouse.point  = doc_point;
    event.data.mouse.button = wuss_BUTTON_SELECT;
    return win->task.handle(win, &event, win->task.task_data);
  }

  return result_OK;
}
