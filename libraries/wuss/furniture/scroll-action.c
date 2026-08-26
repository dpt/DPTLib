/* scroll-action.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss__furniture_scroll_step(wuss_window_t *window, point_t delta)
{
  box_t content;
  int   sx, sy, max_x, max_y;

  wuss_window_get_scroll(window, &sx, &sy);
  wuss__content_box(window, &content);

  max_x = window->doc_width  - (content.x1 - content.x0);
  max_y = window->doc_height - (content.y1 - content.y0);
  if (max_x < 0)
    max_x = 0;
  if (max_y < 0)
    max_y = 0;

  sx += delta.x;
  sy += delta.y;
  if (sx < 0)
    sx = 0;
  else if (sx > max_x)
    sx = max_x;
  if (sy < 0)
    sy = 0;
  else if (sy > max_y)
    sy = max_y;

  wuss_window_set_scroll(window, sx, sy);
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
    wuss_window_set_scroll(window, new_scroll, window->scroll.y);
  else
    wuss_window_set_scroll(window, window->scroll.x, new_scroll);
}
