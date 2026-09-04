/* wuss/furniture/scroll-action.c -- wuss - minimal window manager */

#include "../core/impl.h"

void wuss__furniture_drag_sausage(wuss_window_t *window,
                                  int            delta_px,
                                  int            scroll_start,
                                  int            horizontal)
{
  box_t content;
  int   well_px, track_px, end_gap, content_size, doc_size, max_scroll, new_scroll;

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

  /* Sausage travels over the well minus the cosmetic end gaps; must match
   * wuss__*scroll_sausage_box so drag tracks the pointer 1:1. */
  end_gap  = (well_px > 2 * WUSS_SCROLL_END_GAP + WUSS_MIN_SAUSAGE) ? WUSS_SCROLL_END_GAP : 0;
  track_px = well_px - 2 * end_gap;

  max_scroll = MAX(doc_size - content_size, 0);

  if (track_px <= 0 || doc_size <= content_size)
    new_scroll = 0;
  else
    new_scroll = scroll_start + delta_px * doc_size / track_px;

  new_scroll = CLAMP(new_scroll, 0, max_scroll);

  if (horizontal)
    wuss_window_set_scroll(window, POINT(new_scroll, window->scroll.y));
  else
    wuss_window_set_scroll(window, POINT(window->scroll.x, new_scroll));
}
