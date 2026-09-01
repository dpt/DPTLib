/* wuss/window/get-visible-bounds.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss_window_get_visible_bounds(const wuss_window_t *window, box_t *visible)
{
  *visible = window->visible;
}
