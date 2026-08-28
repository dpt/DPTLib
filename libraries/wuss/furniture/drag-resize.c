/* drag-resize.c -- wuss - minimal window manager */

#include "base/utils.h"

#include "../impl.h"

void wuss__furniture_drag_resize(wuss_window_t *window, point_t p)
{
  box_t content;
  int   width, height;

  wuss__content_box(window, &content);

  width  = p.x - content.x0;
  height = p.y - content.y0;
  width  = CLAMP(width,  WUSS_MIN_CONTENT, window->doc.w);
  height = CLAMP(height, WUSS_MIN_CONTENT, window->doc.h);

  wuss_window_resize(window, (size2d_t) { width, height });
}
