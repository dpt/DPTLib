/* wuss/icon/set-hidden.c -- show or hide a work-area icon */

#include "../core/impl.h"

void wuss_icon_set_hidden(wuss_window_t *window,
                          wuss_icon_t   *icon,
                          int            hidden)
{
  if (hidden && window->wuss->caret_icon == icon)
    wuss__caret_clear(window->wuss);

  if (hidden)
    icon->spec.flags |= wuss_ICON_FLAGS_HIDDEN;
  else
    icon->spec.flags &= (wuss_icon_flags_t) ~wuss_ICON_FLAGS_HIDDEN;

  wuss__icon_invalidate(window, icon);
}
