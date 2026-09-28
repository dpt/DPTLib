/* wuss/iconbar/hit-test.c -- icon bar hit-testing and click dispatch */

#include "geom/box.h"
#include "geom/point.h"

#include "../core/impl.h"

#ifdef WUSS_ICONBAR

wuss_iconbar_icon_t *wuss__iconbar_hit_test(wuss_t *wuss, point_t p)
{
  box_t bar;
  int   index;

  wuss__iconbar_box(wuss, &bar);
  if (!box_contains_point(&bar, p.x, p.y))
    return NULL;

  index = (p.x - bar.x0) / WUSS_ICONBAR_SLOT;
  if (index < 0 || index >= wuss->niconbar_icons)
    return NULL;

  return wuss->iconbar_icons[index];
}

/* Click/drag dispatch, called from wuss_mouse_click before the ordinary
 * window hit-test: the bar draws on top, but only claims a click that lands
 * on one of its icons -- empty slot space (including the whole bar when it
 * has no icons) falls through to whatever window is underneath. Returns 1
 * when the bar handled the event (caller should return without falling
 * through to window dispatch), 0 otherwise. */
int wuss__iconbar_mouse_click(wuss_t             *wuss,
                              point_t             p,
                              wuss_button_t       button,
                              wuss_mouse_action_t action)
{
  wuss_iconbar_icon_t *icon;
  wuss_event_t         event;
  box_t                bar;

  if (action == wuss_MOUSE_UP && wuss->pressed_iconbar_icon != NULL)
  {
    icon = wuss->pressed_iconbar_icon;

    wuss->pressed_iconbar_icon = NULL;
    icon->state &= ~wuss_ICONBAR_ICON_STATE_PRESSED;
    wuss__iconbar_box(wuss, &bar);
    wuss_invalidate(wuss, &bar);

    event.kind                     = wuss_EVENT_ICON;
    event.data.iconbar_icon.icon   = icon;
    event.data.iconbar_icon.action = action;
    event.data.iconbar_icon.button = button;
    (void) wuss__deliver(icon->owner, NULL, &event);
    return 1;
  }

  icon = wuss__iconbar_hit_test(wuss, p);
  if (icon == NULL)
    return 0; /* outside the bar, or over empty slot space */

  if (action == wuss_MOUSE_DOWN)
  {
    wuss->pressed_iconbar_icon = icon;
    icon->state |= wuss_ICONBAR_ICON_STATE_PRESSED;
    wuss__iconbar_box(wuss, &bar);
    wuss_invalidate(wuss, &bar);
  }

  event.kind                     = wuss_EVENT_ICON;
  event.data.iconbar_icon.icon   = icon;
  event.data.iconbar_icon.action = action;
  event.data.iconbar_icon.button = button;
  (void) wuss__deliver(icon->owner, NULL, &event);
  return 1;
}

#endif /* WUSS_ICONBAR */
