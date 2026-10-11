/* wuss/window/get-max-content.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss_window_get_max_content(const wuss_window_t *window, size2d_t *max)
{
  wuss__max_content_anywhere_on_screen(window, max);
}
