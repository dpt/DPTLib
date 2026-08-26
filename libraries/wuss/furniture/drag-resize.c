/* drag-resize.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss__furniture_drag_resize(wuss_window_t *window, int x, int y)
{
  box_t content;
  int   width, height;

  wuss__content_box(window, &content);

  width  = x - content.x0;
  height = y - content.y0;
  if (width > window->doc_width)
    width = window->doc_width;
  if (height > window->doc_height)
    height = window->doc_height;
  if (width < WUSS_MIN_CONTENT)
    width = WUSS_MIN_CONTENT;
  if (height < WUSS_MIN_CONTENT)
    height = WUSS_MIN_CONTENT;

  wuss_window_resize(window, width, height);
}
