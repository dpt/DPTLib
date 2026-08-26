/* scroll-action.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss__furniture_scroll_step(wuss_window_t *window, point_t delta)
{
  box_t   content;
  point_t scroll;
  int     max_x, max_y;

  wuss_window_get_scroll(window, &scroll);
  wuss__content_box(window, &content);

  max_x = window->doc_width  - (content.x1 - content.x0);
  max_y = window->doc_height - (content.y1 - content.y0);
  if (max_x < 0)
    max_x = 0;
  if (max_y < 0)
    max_y = 0;

  scroll.x += delta.x;
  scroll.y += delta.y;
  if (scroll.x < 0)
    scroll.x = 0;
  else if (scroll.x > max_x)
    scroll.x = max_x;
  if (scroll.y < 0)
    scroll.y = 0;
  else if (scroll.y > max_y)
    scroll.y = max_y;

  wuss_window_set_scroll(window, scroll);
}

void wuss__furniture_drag_thumb(wuss_window_t *window,
                                int            delta_px,
                                int            scroll_start,
                                int            horizontal)
{
  box_t content;
  int   track_px, content_size, doc_size, max_scroll, new_scroll;

  wuss__content_box(window, &content);

  if (horizontal)
  {
    track_px     = wuss__hscroll_track_px(window);
    content_size = content.x1 - content.x0;
    doc_size     = window->doc_width;
  }
  else
  {
    track_px     = wuss__vscroll_track_px(window);
    content_size = content.y1 - content.y0;
    doc_size     = window->doc_height;
  }

  max_scroll = doc_size - content_size;
  if (max_scroll < 0)
    max_scroll = 0;

  if (track_px <= 0 || doc_size <= content_size)
    new_scroll = 0;
  else
    new_scroll = scroll_start + delta_px * doc_size / track_px;

  if (new_scroll < 0)
    new_scroll = 0;
  else if (new_scroll > max_scroll)
    new_scroll = max_scroll;

  if (horizontal)
    wuss_window_set_scroll(window, (point_t) { new_scroll, window->scroll.y });
  else
    wuss_window_set_scroll(window, (point_t) { window->scroll.x, new_scroll });
}
