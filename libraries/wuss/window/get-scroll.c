/* get-scroll.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss_window_get_scroll(const wuss_window_t *window, int *x, int *y)
{
  *x = window->scroll_x;
  *y = window->scroll_y;
}
