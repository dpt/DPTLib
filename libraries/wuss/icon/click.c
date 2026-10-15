/* wuss/icon/click.c -- wuss - pointer clicks on icons */

#include "../core/types.h"
#include "../core/geometry.h"
#include "../core/input.h"

void wuss__icon_release_held(wuss_t             *wuss,
                             point_t             p,
                             wuss_mouse_action_t action)
{
  wuss_icon_t   *pressed;
  wuss_window_t *presswin;
  box_t          content;
  point_t        doc_point;
  int            still_over;

  if (action != wuss_MOUSE_UP || wuss->pressed.icon == NULL)
    return;

  pressed  = wuss->pressed.icon;
  presswin = wuss->pressed.window;

  wuss__content_box(presswin, &content);
  doc_point.x = p.x - content.x0 + presswin->scroll.x;
  doc_point.y = p.y - content.y0 + presswin->scroll.y;

  still_over = (wuss__window_at(wuss, p) == presswin &&
                wuss__icon_hit_test(presswin, doc_point) == pressed);

  if (still_over)
    return;

  wuss->pressed.icon   = NULL;
  wuss->pressed.window = NULL;
  if (wuss__icon_pressed(pressed))
  {
    wuss__icon_set_state(pressed, wuss_ICON_STATE_PRESSED, 0);
    wuss__icon_invalidate(presswin, pressed);
  }
}

int wuss__icon_click(wuss_window_t      *window,
                     point_t             p,
                     point_t             doc_point,
                     wuss_button_t       button,
                     wuss_mouse_action_t action,
                     result_t           *rc)
{
  wuss_t      *wuss;
  wuss_icon_t *icon;
  wuss_event_t event;

  wuss = window->wuss;

  /* MENU is never an icon gesture -- press/select/adjust are. Skipping the hit
   * test lets a MENU click over an icon fall through to the plain
   * wuss_EVENT_MOUSE, so the task's own MENU handling (e.g. popping a
   * work-area menu) sees it instead of the click being eaten here. */
  icon = (button & wuss_BUTTON_MENU) ? NULL : wuss__icon_hit_test(window, doc_point);
  if (icon == NULL)
    return 0;

  if (action == wuss_MOUSE_DOWN && (button & WUSS_BUTTON_CLICK))
  {
    wuss__icon_set_state(icon, wuss_ICON_STATE_PRESSED, 1);
    wuss->pressed.icon   = icon;
    wuss->pressed.window = window;
    wuss->pressed.point  = p;
    wuss->pressed.button = button;
    wuss__icon_invalidate(window, icon);

    /* a slider jumps straight to the click point rather than waiting for a
     * completed press/release, and keeps updating on MOVE while held
     * (core/mouse-move.c) */
    if (icon->spec.type == wuss_ICON_TYPE_SLIDER)
      wuss__icon_set_value(window, icon,
                           wuss__slider_value_for_point(window, icon, p));

    /* a writable takes the caret at the click, if the press above gave its
     * window the focus */
    if (icon->spec.type == wuss_ICON_TYPE_WRITABLE && wuss->focus == window)
      wuss__writable_place_caret(window, icon,
                                 wuss__writable_index_for_x(wuss, icon,
                                                            doc_point.x));
  }
  else if (action == wuss_MOUSE_UP && wuss__icon_pressed(icon))
  {
    wuss__icon_set_state(icon, wuss_ICON_STATE_PRESSED, 0);
    wuss->pressed.icon   = NULL;
    wuss->pressed.window = NULL;
    wuss__icon_invalidate(window, icon);

    /* a completed click latches radio/option state before the task is told, so
     * the wuss_EVENT_ICON handler sees the new value */
    if (icon->spec.type == wuss_ICON_TYPE_OPTION)
      wuss__icon_select(window, icon, !wuss__icon_selected(icon));
    else if (icon->spec.type == wuss_ICON_TYPE_BITMAP &&
             icon->spec.u.bitmap.group != 0)
      wuss__icon_select(window, icon, 1);
    else if (icon->spec.type == wuss_ICON_TYPE_RADIO)
      /* ADJUST toggles a lone radio; a grouped one always keeps one member
       * selected */
      wuss__icon_select(window, icon,
                        ((button & wuss_BUTTON_ADJUST) &&
                         icon->spec.u.radio.group == 0)
                          ? !wuss__icon_selected(icon)
                          : 1);
  }

  event.kind             = wuss_EVENT_ICON;
  event.data.icon.icon   = icon;
  event.data.icon.action = action;
  event.data.icon.button = button;
  event.data.icon.value  = icon->value;
  *rc = wuss__deliver(window->task, window, &event);

  return 1;
}
