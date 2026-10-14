/* wuss/icon/set-selected.c -- latch a radio/option icon's selected state */

#include <assert.h>

#include "../core/types.h"

/* the exclusive-selection group of a radio or latching bitmap, else 0 */
static int icon_group(const wuss_icon_t *icon)
{
  if (icon->spec.type == wuss_ICON_TYPE_RADIO)
    return icon->spec.u.radio.group;
  if (icon->spec.type == wuss_ICON_TYPE_BITMAP)
    return icon->spec.u.bitmap.group;

  return 0;
}

void wuss__icon_select(wuss_window_t *window,
                       wuss_icon_t   *icon,
                       int            selected)
{
  wuss_icon_t *other;
  int          i;

  assert(window != NULL);
  assert(icon   != NULL);

  if (icon->spec.type != wuss_ICON_TYPE_RADIO &&
      icon->spec.type != wuss_ICON_TYPE_OPTION &&
      !(icon->spec.type == wuss_ICON_TYPE_BITMAP && icon_group(icon) != 0) &&
      icon->spec.type != wuss_ICON_TYPE_MENU_ENTRY)
    return;

  selected = selected ? 1 : 0;

  /* selecting a grouped radio clears its siblings first */
  if (selected && icon_group(icon) != 0)
  {
    for (i = 0; i < window->nicons; i++)
    {
      other = window->icons[i];
      if (other == icon)
        continue;
      if (other->spec.type != icon->spec.type ||
          icon_group(other) != icon_group(icon))
        continue;
      if (!wuss__icon_selected(other))
        continue;
      wuss__icon_set_state(other, wuss_ICON_STATE_SELECTED, 0);
      wuss__icon_invalidate(window, other);
    }
  }

  if (wuss__icon_selected(icon) == selected)
    return;

  wuss__icon_set_state(icon, wuss_ICON_STATE_SELECTED, selected);
  wuss__icon_invalidate(window, icon);
}

void wuss_icon_set_selected(wuss_window_t *window,
                            wuss_icon_t   *icon,
                            int            selected)
{
  wuss__icon_select(window, icon, selected);
}
