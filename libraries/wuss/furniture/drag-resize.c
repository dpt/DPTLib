/* drag-resize.c -- wuss - minimal window manager */

#include "base/utils.h"

#include "../impl.h"

void wuss__furniture_drag_resize(wuss_window_t *window, point_t p)
{
  box_t    content;
  size2d_t min;
  int      width, height;

  wuss__content_box(window, &content);
  wuss__min_content(window, &min);

  width  = p.x - content.x0;
  height = p.y - content.y0;
  width  = CLAMP(width,  min.w, MAX(window->doc.w, min.w));
  height = CLAMP(height, min.h, MAX(window->doc.h, min.h));

  wuss_window_resize(window, (size2d_t) { width, height });
}
