/* wuss/iconbar/hit-test.c -- icon bar hit-testing and click dispatch */

#include "geom/box.h"
#include "geom/point.h"

#include "../core/types.h"
#include "../core/dispatch.h"

#ifdef WUSS_ICONBAR

wuss_iconbar_icon_t *wuss__iconbar_hit_test(wuss_t *wuss, point_t p)
{
  box_t bar, slot;
  int   i;

  wuss__iconbar_box(wuss, &bar);
  if (!box_contains_point(&bar, p.x, p.y))
    return NULL;

  for (i = 0; i < wuss->iconbar.n; i++)
  {
    wuss__iconbar_slot_box(wuss, i, &slot);
    if (box_contains_point(&slot, p.x, p.y))
      return wuss->iconbar.icons[i];
  }

  return NULL; /* over the gap or bare bar */
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
  }
  else
  {
    icon = wuss__iconbar_hit_test(wuss, p);
    if (icon == NULL)
      return 0; /* over empty slot space */

    if (action == wuss_MOUSE_DOWN)
      wuss->iconbar.pressed = icon;
  }

  event.kind                     = wuss_EVENT_ICON;
  event.data.iconbar_icon.icon   = icon;
  event.data.iconbar_icon.action = action;
  event.data.iconbar_icon.button = button;
  (void) wuss__deliver(icon->owner, NULL, &event);
  return 1;
}

#endif /* WUSS_ICONBAR */
