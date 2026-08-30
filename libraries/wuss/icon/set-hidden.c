/* set-hidden.c -- wuss - show or hide a work-area icon */

#include "../impl.h"

void wuss_icon_set_hidden(wuss_icon_t *icon, int hidden)
{
  if (hidden)
    icon->flags |= wuss_ICON_FLAGS_HIDDEN;
  else
    icon->flags &= (wuss_icon_flags_t) ~wuss_ICON_FLAGS_HIDDEN;

  wuss__icon_invalidate(icon);
}
