/* wuss/mouse-move.c -- wuss - minimal window manager */

#include "impl.h"

result_t wuss_mouse_move(wuss_t *wuss, point_t p, wuss_window_t **hit)
{
  wuss_window_t *win;
  int            x, y;

  /* Clamp inbound coordinates to the screen so a drag can never carry a
   * window off the desktop edges on a pointer report from outside the frame. */
  x = CLAMP(p.x, 0, wuss->scr->size.w - 1);
  y = CLAMP(p.y, 0, wuss->scr->size.h - 1);

  p.x = x;
  p.y = y;

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
      wuss->furniture_ops->drag_resize(win, POINT(x, y));
      break;

    case wuss_FURNITURE_DRAG_VSCROLL_SAUSAGE:
      wuss->furniture_ops->drag_sausage(win, y - wuss->furniture.drag.y, wuss->furniture.drag_scroll_start, 0);
      break;

    case wuss_FURNITURE_DRAG_HSCROLL_SAUSAGE:
      wuss->furniture_ops->drag_sausage(win, x - wuss->furniture.drag.x, wuss->furniture.drag_scroll_start, 1);
      break;

    case wuss_FURNITURE_DRAG_MOVE:
      wuss_window_move(win, POINT(x - wuss->furniture.drag.x, y - wuss->furniture.drag.y));
      break;

    case wuss_FURNITURE_DRAG_NONE:
    default:
      /* A pressed scroll arrow or resize icon arms "dragging" purely so
       * MOUSE_UP's generic release path can clear the press highlight; it is
       * not a real drag and must not move the window on every mouse move
       * until the button is released. */
      break;
    }

    return result_OK;
  }
#endif

  win = wuss__window_at(wuss, p);
  if (hit != NULL)
    *hit = win;

  /* Whole-footprint enter/exit tracking, before any furniture/no-handler
   * early return -- a furniture hover still counts as "inside the window". */
  wuss__pointer_set_window(wuss, win);

  if (win == NULL)
  {
#ifdef WUSS_ICONS
    wuss__icon_set_hover(wuss, NULL, NULL);
#endif
    return result_OK;
  }

#ifdef WUSS_FURNITURE
  if (wuss->furniture_ops->hit_test(win, POINT(x, y)) != wuss_FURNITURE_CONTENT)
  {
#ifdef WUSS_ICONS
    wuss__icon_set_hover(wuss, NULL, NULL);
#endif
    return result_OK;
  }
#endif

  if (win->task->handle == NULL)
  {
#ifdef WUSS_ICONS
    wuss__icon_set_hover(wuss, NULL, NULL);
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
    /* a slider drag keeps tracking the pointer even once it strays outside
     * the icon's own bbox, matching a furniture sausage drag */
    if (wuss->pressed_icon != NULL && wuss->pressed_window == win &&
        wuss->pressed_icon->spec.type == wuss_ICON_TYPE_SLIDER)
    {
      wuss_icon_t *icon = wuss->pressed_icon;

      wuss__icon_set_value(win, icon,
                           wuss__slider_value_for_point(win, icon,
                                                        POINT(x, y)));

      event.kind             = wuss_EVENT_ICON;
      event.data.icon.icon   = icon;
      event.data.icon.action = wuss_MOUSE_MOVE;
      event.data.icon.button = wuss_BUTTON_SELECT;
      event.data.icon.value  = icon->value;
      return wuss__deliver(win->task, win, &event);
    }
#endif

#ifdef WUSS_ICONS
    {
      wuss_icon_t *icon;
      int          k;

      icon = wuss__icon_hit_test(win, doc_point);

      wuss__icon_set_hover(wuss, win, icon);

      /* Clear the pressed state of any button the pointer has left. This does
       * not re-press a button on drag-back-in, and does not track which mouse
       * button is held -- wuss keeps no persistent "button down over content"
       * state. */
      for (k = 0; k < win->nicons; k++)
      {
        wuss_icon_t *it = win->icons[k];

        if (wuss__icon_pressed(it) && it != icon)
        {
          wuss__icon_set_state(it, wuss_ICON_STATE_PRESSED, 0);
          if (wuss->pressed_icon == it)
          {
            wuss->pressed_icon   = NULL;
            wuss->pressed_window = NULL;
          }
          wuss__icon_invalidate(win, it);
        }
      }

      /* a slider only wants MOVE while its own drag is tracked above; without
       * this it would also get one on every plain hover, with no button
       * actually held. Other icon types (menu rows, buttons) rely on this
       * hover MOVE to light up/open on mouse-over, so keep it for them. */
      if (icon != NULL &&
          (icon->spec.type != wuss_ICON_TYPE_SLIDER || icon == wuss->pressed_icon))
      {
        event.kind             = wuss_EVENT_ICON;
        event.data.icon.icon   = icon;
        event.data.icon.action = wuss_MOUSE_MOVE;
        event.data.icon.button = wuss_BUTTON_SELECT;
        event.data.icon.value  = icon->value;
        return wuss__deliver(win->task, win, &event);
      }
    }
#endif

    event.kind              = wuss_EVENT_MOUSE;
    event.data.mouse.action = wuss_MOUSE_MOVE;
    event.data.mouse.point  = doc_point;
    event.data.mouse.button = wuss_BUTTON_SELECT;
    return wuss__deliver(win->task, win, &event);
  }

  return result_OK;
}
