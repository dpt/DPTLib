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

  if (p.x - bar.x0 < WUSS_ICONBAR_GAP)
    return NULL; /* over the bar's leading gap */

  index = (p.x - bar.x0 - WUSS_ICONBAR_GAP) / WUSS_ICONBAR_SLOT;
  if (index < 0 || index >= wuss->iconbar.n)
    return NULL;

  return wuss->iconbar.icons[index];
}

/* Click/drag dispatch, called from wuss_mouse_click once the point has
 * already resolved to wuss->iconbar.window: only claims a click that lands
 * on one of its icons -- empty slot space is a no-op, same as clicking a
 * window's bare content elsewhere. Returns 1 when an icon claimed the
 * event, 0 for empty slot space. */
int wuss__iconbar_icon_click(wuss_t             *wuss,
                             point_t             p,
                             wuss_button_t       button,
                             wuss_mouse_action_t action)
{
  wuss_iconbar_icon_t *icon;
  wuss_event_t         event;

  /* the release goes to the icon pressed, wherever it lands; icon bar icons
   * are click-only, so nothing is drawn for the press itself */
  if (action == wuss_MOUSE_UP && wuss->iconbar.pressed != NULL)
  {
    icon = wuss->iconbar.pressed;

    wuss->iconbar.pressed = NULL;

    event.kind                     = wuss_EVENT_ICON;
    event.data.iconbar_icon.icon   = icon;
    event.data.iconbar_icon.action = action;
    event.data.iconbar_icon.button = button;
    (void) wuss__deliver(icon->owner, NULL, &event);
    return 1;
  }

  icon = wuss__iconbar_hit_test(wuss, p);
  if (icon == NULL)
    return 0; /* over empty slot space */

  if (action == wuss_MOUSE_DOWN)
    wuss->iconbar.pressed = icon;

  event.kind                     = wuss_EVENT_ICON;
  event.data.iconbar_icon.icon   = icon;
  event.data.iconbar_icon.action = action;
  event.data.iconbar_icon.button = button;
  (void) wuss__deliver(icon->owner, NULL, &event);
  return 1;
}

#endif /* WUSS_ICONBAR */
