/* wuss/icon/set-disabled.c -- shade or unshade a work-area icon */

#include "../core/impl.h"

void wuss_icon_set_disabled(wuss_window_t *window,
                            wuss_icon_t   *icon,
                            int            disabled)
{
  wuss__icon_set_flag(window, icon, wuss_ICON_FLAGS_DISABLED, disabled);
}
