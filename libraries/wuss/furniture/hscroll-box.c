/* hscroll-box.c -- wuss - minimal window manager */

#include "../impl.h"

static void hscroll_row(const wuss_window_t *window, box_t *out)
{
  int outline_px, size;

  outline_px = wuss__outline_px(window);
  size       = wuss__icon_size(window);

  out->y1 = window->visible.y1 - outline_px;
  out->y0 = out->y1 - size;

  out->x0 = window->visible.x0 + outline_px;
  out->x1 = window->visible.x1 - outline_px - size; /* stop left of the resize corner */
}

void wuss__hscroll_left_box(const wuss_window_t *window, box_t *out)
{
  box_t row;
  int   size;

  hscroll_row(window, &row);
  size = wuss__icon_size(window);

  out->y0 = row.y0;
  out->y1 = row.y1;
  out->x0 = row.x0;
  out->x1 = row.x0 + size;
}

void wuss__hscroll_right_box(const wuss_window_t *window, box_t *out)
{
  box_t row;
  int   size;

  hscroll_row(window, &row);
  size = wuss__icon_size(window);

  out->y0 = row.y0;
  out->y1 = row.y1;
  out->x1 = row.x1;
  out->x0 = row.x1 - size;
}

void wuss__hscroll_bar_box(const wuss_window_t *window, box_t *out)
{
  box_t row;
  int   size;

  hscroll_row(window, &row);
  size = wuss__icon_size(window);

  out->y0 = row.y0;
  out->y1 = row.y1;
  out->x0 = row.x0 + size;
  out->x1 = row.x1 - size;
}

int wuss__hscroll_track_px(const wuss_window_t *window)
{
  box_t bar;

  wuss__hscroll_bar_box(window, &bar);

  return bar.x1 - bar.x0;
}

void wuss__hscroll_thumb_box(const wuss_window_t *window, box_t *out)
{
  box_t bar, content;
  int   track_px, content_size, doc_size, thumb_px, thumb_x0;

  wuss__hscroll_bar_box(window, &bar);
  wuss__content_box(window, &content);

  track_px     = bar.x1 - bar.x0;
  content_size = content.x1 - content.x0;
  doc_size     = window->doc_width;
  if (doc_size < content_size)
    doc_size = content_size;

  thumb_px = (doc_size > 0) ? track_px * content_size / doc_size : track_px;
  if (thumb_px < WUSS_MIN_THUMB)
    thumb_px = WUSS_MIN_THUMB;
  if (thumb_px > track_px)
    thumb_px = track_px;

  if (doc_size > content_size && track_px > thumb_px)
    thumb_x0 = bar.x0 + (track_px - thumb_px) * window->scroll.x / (doc_size - content_size);
  else
    thumb_x0 = bar.x0;

  out->y0 = bar.y0;
  out->y1 = bar.y1;
  out->x0 = thumb_x0;
  out->x1 = thumb_x0 + thumb_px;
}
