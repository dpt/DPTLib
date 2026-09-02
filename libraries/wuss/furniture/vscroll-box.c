/* wuss/furniture/vscroll-box.c -- wuss - minimal window manager */

#include "../impl.h"

static void vscroll_column(const wuss_window_t *window, box_t *out)
{
  box_t titlebar;
  int   outline_px, size;

  outline_px = wuss__outline_px(window);
  size       = wuss__button_size(window);

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
  size = wuss__button_size(window);

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
  size = wuss__button_size(window);

  out->x0 = column.x0;
  out->x1 = column.x1;
  out->y1 = column.y1;
  out->y0 = column.y1 - size;
}

void wuss__vscroll_well_box(const wuss_window_t *window, box_t *out)
{
  box_t column;
  int   size;

  vscroll_column(window, &column);
  size = wuss__button_size(window);

  out->x0 = column.x0;
  out->x1 = column.x1;
  out->y0 = column.y0 + size;
  out->y1 = column.y1 - size;
}

int wuss__vscroll_well_px(const wuss_window_t *window)
{
  box_t well;

  wuss__vscroll_well_box(window, &well);

  return well.y1 - well.y0;
}

void wuss__vscroll_sausage_box(const wuss_window_t *window, box_t *out)
{
  box_t well, content;
  int   well_px, track_px, end_gap, content_size, doc_size, sausage_px, sausage_y0, inset;

  wuss__vscroll_well_box(window, &well);
  wuss__content_box(window, &content);

  well_px = well.y1 - well.y0;

  /* Leave a cosmetic gap at each end of the well; drop it if the well is
   * too short to spare two gaps plus a minimum sausage. */
  end_gap  = (well_px > 2 * WUSS_SCROLL_END_GAP + WUSS_MIN_SAUSAGE) ? WUSS_SCROLL_END_GAP : 0;
  track_px = well_px - 2 * end_gap;

  content_size = content.y1 - content.y0;
  doc_size     = window->doc.h;
  if (doc_size < content_size)
    doc_size = content_size;

  sausage_px = (doc_size > 0) ? track_px * content_size / doc_size : track_px;
  if (sausage_px < WUSS_MIN_SAUSAGE)
    sausage_px = WUSS_MIN_SAUSAGE;
  if (sausage_px > track_px)
    sausage_px = track_px;

  if (doc_size > content_size && track_px > sausage_px)
    sausage_y0 = well.y0 + end_gap + (track_px - sausage_px) * window->scroll.y / (doc_size - content_size);
  else
    sausage_y0 = well.y0 + end_gap;

  /* Inset the sausage from the well's long edges, but only while that
   * leaves something to draw: a tiny titlebar font can make the well
   * narrower than two insets, which would invert the box. */
  inset = (well.x1 - well.x0 > 2 * WUSS_SCROLL_INSET) ? WUSS_SCROLL_INSET : 0;
  out->x0 = well.x0 + inset;
  out->x1 = well.x1 - inset;
  out->y0 = sausage_y0;
  out->y1 = sausage_y0 + sausage_px;
}
