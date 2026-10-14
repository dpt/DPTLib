/* wuss/mouse-click.c -- wuss - minimal window manager */

#include "impl.h"

static result_t mouse_click(wuss_t             *wuss,
                            point_t             p,
                            wuss_button_t       button,
                            wuss_mouse_action_t action,
                            wuss_window_t     **hit)
{
  result_t       rc;
  wuss_window_t *win;
  wuss_window_t *held;
  wuss_event_t   event;
  int            x, y;

  x = p.x;
  y = p.y;

  wuss->pointer = p;

  /* Core owns all input while a drag is active: releasing the button that
   * started it (any button, if unknown) ends it, dropping on whatever's under
   * the pointer; any other click is swallowed rather than reaching a window
   * underneath the ants. */
  if (wuss->drag.window != NULL)
  {
    win = wuss__window_at(wuss, p);
    if (hit != NULL)
      *hit = win;

    if (action == wuss_MOUSE_UP &&
        (wuss->drag.button == wuss_BUTTON_NONE ||
         (button & wuss->drag.button) != 0))
      wuss__drag_end(wuss, p, win, 0);

    return result_OK;
  }

  /* A captured press owns the pointer until released: a further press or the
   * release goes to the capturing window, wherever it lands. */
  if (wuss->capture != NULL)
  {
    if (hit != NULL)
      *hit = wuss->capture;

    return wuss__capture_deliver(wuss, p, button, action);
  }

  if (action == wuss_MOUSE_DOWN)
    button = wuss__double_click(wuss, wuss__window_at(wuss, p), p, button);

  wuss__icon_release_held(wuss, p, action);

  held = wuss__furniture_release(wuss, action);
  if (held != NULL)
  {
    if (hit != NULL)
      *hit = held;

    return result_OK;
  }

  win = wuss__window_at(wuss, p);
  if (hit != NULL)
    *hit = win;

  /* A click can land the pointer on a window without a preceding move
   * (window opened under it, or a synthetic tap); keep enter/exit balanced. */
  wuss__pointer_set_window(wuss, win);

  if (wuss__menu_press(wuss, action, button, win))
    return result_OK;

  if (win == NULL)
    return result_OK;

  if (wuss__furniture_click(win, p, button, action, &rc))
    return rc;

  /* A Select/Adjust press on a focusable window's content takes the input
   * focus, before the press itself is delivered. Menu never moves it. */
  if (action == wuss_MOUSE_DOWN                          &&
      (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)) &&
      (win->flags & wuss_WINDOW_FOCUSABLE))
    (void) wuss_set_focus(wuss, win);

#ifdef WUSS_ICONBAR
  if (win == wuss->iconbar.window)
  {
    (void) wuss__iconbar_icon_click(wuss, p, button, action);
    return result_OK;
  }
#endif

  if (win->task->handle != NULL)
  {
    box_t   content;
    point_t doc_point;

    wuss__content_box(win, &content);
    doc_point.x = x - content.x0 + win->scroll.x;
    doc_point.y = y - content.y0 + win->scroll.y;

    if (wuss__icon_click(win, p, doc_point, button, action, &rc))
      return rc;

    /* Set before delivery: closing the window in the handler clears it. */
    if (action == wuss_MOUSE_DOWN &&
        (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
      wuss->capture = win;

    event.kind              = wuss_EVENT_MOUSE;
    event.data.mouse.action = action;
    event.data.mouse.point  = doc_point;
    event.data.mouse.button = button;
    return wuss__deliver(win->task, win, &event);
  }

  return result_OK;
}

result_t wuss_mouse_click(wuss_t             *wuss,
                          point_t             p,
                          wuss_button_t       button,
                          wuss_mouse_action_t action,
                          wuss_window_t     **hit)
{
  result_t rc;

  wuss__message_enter(wuss);
  rc = mouse_click(wuss, p, button, action, hit);
  wuss__message_leave(wuss);

  return rc;
}

void wuss_set_modifiers(wuss_t *wuss, wuss_key_modifiers_t modifiers)
{
  wuss->modifiers = modifiers;
}
