/* get-content-bounds.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss_window_get_content_bounds(const wuss_window_t *window, box_t *content)
{
  wuss__content_box(window, content);
}
