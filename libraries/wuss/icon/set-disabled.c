/* wuss/icon/set-disabled.c -- shade or unshade a work-area icon */

#include "../core/impl.h"

void wuss_icon_set_disabled(wuss_window_t *window,
                            wuss_icon_t   *icon,
                            int            disabled)
{
  if (disabled && window->wuss->caret_icon == icon)
    wuss__caret_clear(window->wuss);

  if (disabled)
    icon->spec.flags |= wuss_ICON_FLAGS_DISABLED;
  else
    icon->spec.flags &= (wuss_icon_flags_t) ~wuss_ICON_FLAGS_DISABLED;

  wuss__icon_invalidate(window, icon);
}
