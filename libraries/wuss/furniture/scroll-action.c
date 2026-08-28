/* scroll-action.c -- wuss - minimal window manager */

#include "../impl.h"

point_t wuss__scroll_clamp(const wuss_window_t *window, point_t desired)
{
  box_t content;
  int   max_x, max_y;

  wuss__content_box(window, &content);

  max_x = window->doc.w  - (content.x1 - content.x0);
  max_y = window->doc.h - (content.y1 - content.y0);
  if (max_x < 0)
    max_x = 0;
  if (max_y < 0)
    max_y = 0;

  if (desired.x < 0)
    desired.x = 0;
  else if (desired.x > max_x)
    desired.x = max_x;
  if (desired.y < 0)
    desired.y = 0;
  else if (desired.y > max_y)
    desired.y = max_y;

  return desired;
}

void wuss__furniture_scroll_step(wuss_window_t *window, point_t delta)
{
  point_t scroll;

  wuss_window_get_scroll(window, &scroll);
  scroll.x += delta.x;
  scroll.y += delta.y;
  scroll = wuss__scroll_clamp(window, scroll);

  wuss_window_set_scroll(window, scroll);
}

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
    wuss_window_set_scroll(window, (point_t) { new_scroll, window->scroll.y });
  else
    wuss_window_set_scroll(window, (point_t) { window->scroll.x, new_scroll });
}
