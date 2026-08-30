/* get-window.c -- wuss - read a work-area icon's owning window */

#include "../impl.h"

wuss_window_t *wuss_icon_get_window(const wuss_icon_t *icon)
{
  return icon->window;
}
