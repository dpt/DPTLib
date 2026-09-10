/* wuss/icon/set-selected.c -- latch a radio/option icon's selected state */

#include <assert.h>

#include "../core/impl.h"

void wuss__icon_select(wuss_window_t *window,
                       wuss_icon_t   *icon,
                       int            selected)
{
  wuss_icon_t *other;
  int          i;

  assert(window != NULL);
  assert(icon   != NULL);

  if (icon->type != wuss_ICON_TYPE_RADIO &&
      icon->type != wuss_ICON_TYPE_OPTION &&
      icon->type != wuss_ICON_TYPE_MENU_ENTRY)
    return;

  selected = selected ? 1 : 0;

  /* selecting a grouped radio clears its siblings first */
  if (selected &&
      icon->type == wuss_ICON_TYPE_RADIO &&
      icon->u.radio.group != 0)
  {
    for (i = 0; i < window->nicons; i++)
    {
      other = window->icons[i];
      if (other == icon)
        continue;
      if (other->type != wuss_ICON_TYPE_RADIO ||
          other->u.radio.group != icon->u.radio.group)
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
