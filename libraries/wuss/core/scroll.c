/* wuss/scroll.c -- wuss - minimal window manager */

#include <stdlib.h>

#include "impl.h"

/* Scrolling moves content under a stationary pointer, so the icon the pointer
 * now sits over may differ from before. Re-resolve the hovered icon so a
 * highlighted wuss_ICON_TYPE_MENU_ENTRY does not keep its highlight after
 * scrolling out from under the pointer. */
static void wuss__scroll_rehover(wuss_t        *wuss,
                                 wuss_window_t *win,
                                 point_t        screen_point)
{
#ifdef WUSS_ICONS
  box_t   content;
  point_t doc_point;

  wuss__content_box(win, &content);
  doc_point.x = screen_point.x - content.x0 + win->scroll.x;
  doc_point.y = screen_point.y - content.y0 + win->scroll.y;

  wuss__icon_set_hover(wuss, win, wuss__icon_hit_test(win, doc_point));
#else
  (void) wuss;
  (void) win;
  (void) screen_point;
#endif
}

#ifdef WUSS_ICONS
/* Bump slider "icon" of "win" by "delta" wheel notches of spec.u.slider.step
 * (0 meaning 1), a positive delta moving towards max, and tell the task. */
static result_t wuss__scroll_slider(wuss_window_t *win,
                                    wuss_icon_t   *icon,
                                    int            delta)
{
  int          step;
  wuss_event_t event;

  step = icon->spec.u.slider.step ? abs(icon->spec.u.slider.step) : 1;
  if (icon->spec.u.slider.min > icon->spec.u.slider.max)
    step = -step;

  wuss__icon_set_value(win, icon, icon->value + delta * step);

  event.kind             = wuss_EVENT_ICON;
  event.data.icon.icon   = icon;
  event.data.icon.action = wuss_MOUSE_MOVE;
  event.data.icon.button = wuss_BUTTON_NONE;
  event.data.icon.value  = icon->value;
  return wuss__deliver(win->task, win, &event);
}

/* The topmost visible, enabled slider of "win" whose full bbox -- groove and
 * decorative surround alike, unlike wuss__icon_hit_test -- contains
 * "doc_point", or NULL. */
static wuss_icon_t *wuss__scroll_slider_at(wuss_window_t *win,
                                           point_t        doc_point)
{
  int          i;
  wuss_icon_t *it;

  for (i = win->nicons - 1; i >= 0; i--)
  {
    it = win->icons[i];
    if (it->spec.type != wuss_ICON_TYPE_SLIDER ||
        (it->spec.flags & (wuss_ICON_FLAGS_HIDDEN | wuss_ICON_FLAGS_DISABLED)))
      continue;

    if (box_contains_point(&it->spec.bbox, doc_point.x, doc_point.y))
      return it;
  }

  return NULL;
}
#endif

result_t wuss_scroll(wuss_t *wuss, point_t p, int delta, wuss_window_t **hit)
{
  wuss_window_t *win;
  int            x, y;

  x = p.x;
  y = p.y;

  win = wuss__window_at(wuss, p);
  if (hit != NULL)
    *hit = win;

  /* core owns all input while a drag is active, as for clicks and keys */
  if (win == NULL || wuss->drag_window != NULL)
    return result_OK;

#ifdef WUSS_FURNITURE
  {
    box_t titlebar;

    wuss__titlebar_box(win, &titlebar);
    if (box_contains_point(&titlebar, x, y))
      return result_OK;
  }
#endif

  {
    box_t        content;
    wuss_event_t event;

    /* Note the pointer position before scrolling, so the event describes the
     * content the pointer was over when the wheel turned, not the content the
     * step below brings under it. */
    wuss__content_box(win, &content);
    event.kind                = wuss_EVENT_SCROLL;
    event.data.scroll.point.x = x - content.x0 + win->scroll.x;
    event.data.scroll.point.y = y - content.y0 + win->scroll.y;
    event.data.scroll.delta   = delta;

#ifdef WUSS_ICONS
    {
      wuss_icon_t *icon;

      /* a wheel anywhere over a slider bumps its value instead of scrolling
       * the window, raised as a MOVE just like a drag step */
      icon = wuss__scroll_slider_at(win, event.data.scroll.point);
      if (icon != NULL)
        return wuss__scroll_slider(win, icon, delta);
    }
#endif

    wuss__scroll_step(win, POINT(0, delta));
    wuss__scroll_rehover(wuss, win, POINT(x, y));

    return wuss__deliver(win->task, win, &event);
  }
}
