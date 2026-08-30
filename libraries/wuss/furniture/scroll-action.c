/* scroll-action.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss__furniture_drag_sausage(wuss_window_t *window,
                                  int            delta_px,
                                  int            scroll_start,
                                  int            horizontal)
{
  box_t content;
  int   well_px, content_size, doc_size, max_scroll, new_scroll;

  wuss__content_box(window, &content);

  if (horizontal)
  {
    well_px      = wuss__hscroll_well_px(window);
    content_size = content.x1 - content.x0;
    doc_size     = window->doc.w;
  }
  else
  {
    well_px      = wuss__vscroll_well_px(window);
    content_size = content.y1 - content.y0;
    doc_size     = window->doc.h;
  }

  max_scroll = doc_size - content_size;
  if (max_scroll < 0)
    max_scroll = 0;

  if (well_px <= 0 || doc_size <= content_size)
    new_scroll = 0;
  else
    new_scroll = scroll_start + delta_px * doc_size / well_px;

  if (new_scroll < 0)
    new_scroll = 0;
  else if (new_scroll > max_scroll)
    new_scroll = max_scroll;

  if (horizontal)
    wuss_window_set_scroll(window, POINT(new_scroll, window->scroll.y));
  else
    wuss_window_set_scroll(window, POINT(window->scroll.x, new_scroll));
}
