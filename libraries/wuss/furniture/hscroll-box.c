/* wuss/furniture/hscroll-box.c -- wuss - minimal window manager */

#include "../impl.h"

static void hscroll_row(const wuss_window_t *window, box_t *out)
{
  int outline_px, size;

  outline_px = wuss__outline_px(window);
  size       = wuss__button_size(window);

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
  size = wuss__button_size(window);

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
  size = wuss__button_size(window);

  out->y0 = row.y0;
  out->y1 = row.y1;
  out->x1 = row.x1;
  out->x0 = row.x1 - size;
}

void wuss__hscroll_well_box(const wuss_window_t *window, box_t *out)
{
  box_t row;
  int   size;

  hscroll_row(window, &row);
  size = wuss__button_size(window);

  out->y0 = row.y0;
  out->y1 = row.y1;
  out->x0 = row.x0 + size;
  out->x1 = row.x1 - size;
}

int wuss__hscroll_well_px(const wuss_window_t *window)
{
  box_t well;

  wuss__hscroll_well_box(window, &well);

  return well.x1 - well.x0;
}

void wuss__hscroll_sausage_box(const wuss_window_t *window, box_t *out)
{
  box_t well, content;
  int   well_px, track_px, end_gap, content_size, doc_size, sausage_px, sausage_x0, inset;

  wuss__hscroll_well_box(window, &well);
  wuss__content_box(window, &content);

  well_px = well.x1 - well.x0;

  /* Leave a cosmetic gap at each end of the well; drop it if the well is
   * too short to spare two gaps plus a minimum sausage. */
  end_gap  = (well_px > 2 * WUSS_SCROLL_END_GAP + WUSS_MIN_SAUSAGE) ? WUSS_SCROLL_END_GAP : 0;
  track_px = well_px - 2 * end_gap;

  content_size = content.x1 - content.x0;
  doc_size     = window->doc.w;
  if (doc_size < content_size)
    doc_size = content_size;

  sausage_px = (doc_size > 0) ? track_px * content_size / doc_size : track_px;
  if (sausage_px < WUSS_MIN_SAUSAGE)
    sausage_px = WUSS_MIN_SAUSAGE;
  if (sausage_px > track_px)
    sausage_px = track_px;

  if (doc_size > content_size && track_px > sausage_px)
    sausage_x0 = well.x0 + end_gap + (track_px - sausage_px) * window->scroll.x / (doc_size - content_size);
  else
    sausage_x0 = well.x0 + end_gap;

  /* Inset the sausage from the well's long edges, but only while that
   * leaves something to draw: a tiny titlebar font can make the well
   * narrower than two insets, which would invert the box. */
  inset = (well.y1 - well.y0 > 2 * WUSS_SCROLL_INSET) ? WUSS_SCROLL_INSET : 0;
  out->y0 = well.y0 + inset;
  out->y1 = well.y1 - inset;
  out->x0 = sausage_x0;
  out->x1 = sausage_x0 + sausage_px;
}
