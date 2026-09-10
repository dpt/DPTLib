/* wuss/icon/invalidate.c -- mark a work-area icon's bbox dirty */

#include "../core/impl.h"

void wuss__icon_invalidate(wuss_window_t *window, const wuss_icon_t *icon)
{
  /* the icon's bbox is already in the window-local (pre-scroll) coordinates
   * wuss_window_invalidate expects */
  wuss_window_invalidate(window, &icon->bbox);
}
