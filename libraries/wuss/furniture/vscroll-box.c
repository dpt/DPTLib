/* vscroll-box.c -- wuss - minimal window manager */

#include "../impl.h"

static void vscroll_column(const wuss_window_t *window, box_t *out)
{
  box_t titlebar;
  int   outline_px, size;

  outline_px = wuss__outline_px(window);
  size       = wuss__icon_size(window);

  out->x1 = window->visible.x1 - outline_px;
  out->x0 = out->x1 - size;

  wuss__titlebar_box(window, &titlebar);
  out->y0 = titlebar.y1;
  out->y1 = window->visible.y1 - outline_px - size; /* stop above the resize corner */
}

void wuss__vscroll_up_box(const wuss_window_t *window, box_t *out)
{
  box_t column;
  int   size;

  vscroll_column(window, &column);
  size = wuss__icon_size(window);

  out->x0 = column.x0;
  out->x1 = column.x1;
  out->y0 = column.y0;
  out->y1 = column.y0 + size;
}

void wuss__vscroll_down_box(const wuss_window_t *window, box_t *out)
{
  box_t column;
  int   size;

  vscroll_column(window, &column);
  size = wuss__icon_size(window);

  out->x0 = column.x0;
  out->x1 = column.x1;
  out->y1 = column.y1;
  out->y0 = column.y1 - size;
}

void wuss__vscroll_bar_box(const wuss_window_t *window, box_t *out)
{
  box_t column;
  int   size;

  vscroll_column(window, &column);
  size = wuss__icon_size(window);

  out->x0 = column.x0;
  out->x1 = column.x1;
  out->y0 = column.y0 + size;
  out->y1 = column.y1 - size;
}

int wuss__vscroll_track_px(const wuss_window_t *window)
{
  box_t bar;

  wuss__vscroll_bar_box(window, &bar);

  return bar.y1 - bar.y0;
}

void wuss__vscroll_thumb_box(const wuss_window_t *window, box_t *out)
{
  box_t bar, content;
  int   track_px, content_size, doc_size, thumb_px, thumb_y0;

  wuss__vscroll_bar_box(window, &bar);
  wuss__content_box(window, &content);

  track_px     = bar.y1 - bar.y0;
  content_size = content.y1 - content.y0;
  doc_size     = window->doc_height;
  if (doc_size < content_size)
    doc_size = content_size;

  thumb_px = (doc_size > 0) ? track_px * content_size / doc_size : track_px;
  if (thumb_px < WUSS_MIN_THUMB)
    thumb_px = WUSS_MIN_THUMB;
  if (thumb_px > track_px)
    thumb_px = track_px;

  if (doc_size > content_size && track_px > thumb_px)
    thumb_y0 = bar.y0 + (track_px - thumb_px) * window->scroll_y / (doc_size - content_size);
  else
    thumb_y0 = bar.y0;

  out->x0 = bar.x0;
  out->x1 = bar.x1;
  out->y0 = thumb_y0;
  out->y1 = thumb_y0 + thumb_px;
}
