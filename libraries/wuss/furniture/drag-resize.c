/* drag-resize.c -- wuss - minimal window manager */

#include "base/utils.h"

#include "../impl.h"

void wuss__furniture_drag_resize(wuss_window_t *window, int x, int y)
{
  box_t content;
  int   width, height;

  wuss__content_box(window, &content);

  width  = x - content.x0;
  height = y - content.y0;
  width  = CLAMP(width,  WUSS_MIN_CONTENT, window->doc_width);
  height = CLAMP(height, WUSS_MIN_CONTENT, window->doc_height);

  wuss_window_resize(window, width, height);
}
