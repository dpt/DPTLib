/* wuss/icon/set-hidden.c -- show or hide a work-area icon */

#include "../core/impl.h"

void wuss_icon_set_hidden(wuss_window_t *window,
                          wuss_icon_t   *icon,
                          int            hidden)
{
  wuss__icon_set_flag(window, icon, wuss_ICON_FLAGS_HIDDEN, hidden);
}
