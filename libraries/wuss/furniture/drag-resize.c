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

  /* Subtract the offset recorded at drag start so the point originally
   * grabbed on the resize icon stays under the pointer, rather than the
   * window's edge snapping to meet the pointer on the first move. */
  width  = p.x - window->wuss->furniture.drag_offset.x - content.x0;
  height = p.y - window->wuss->furniture.drag_offset.y - content.y0;
  width  = CLAMP(width,  min.w, MAX(window->doc.w, min.w));
  height = CLAMP(height, min.h, MAX(window->doc.h, min.h));

  wuss_window_resize(window, SIZE2D(width, height));
}
