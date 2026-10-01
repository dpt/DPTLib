/* wuss/icon/set-flag.c -- set or clear a single work-area icon flag */

#include "../core/impl.h"

void wuss__icon_set_flag(wuss_window_t    *window,
                         wuss_icon_t      *icon,
                         wuss_icon_flags_t flag,
                         int               on)
{
  if (on && window->wuss->caret_icon == icon)
    wuss__caret_clear(window->wuss);

  if (on)
    icon->spec.flags |= flag;
  else
    icon->spec.flags &= (wuss_icon_flags_t) ~flag;

  wuss__icon_invalidate(window, icon);
}
