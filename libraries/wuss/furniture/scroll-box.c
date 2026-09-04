/* wuss/furniture/scroll-box.c -- wuss - minimal window manager */

/* The vertical and horizontal scrollbars are the same layout transposed: a
 * fixed-breadth strip down the right / along the bottom of the visible box,
 * an arrow at each end and the well between them, with a proportional sausage
 * in the well. Everything here is written once against a "horizontal" flag;
 * the ten public wuss__vscroll_ / wuss__hscroll_ entry points are thin
 * wrappers that pin the flag. */

#include "../core/impl.h"

/* The scrollbar strip: full breadth, running between the titlebar (v) or left
 * outline (h) and the resize corner. "long" axis is y when vertical, x when
 * horizontal. */
static void scroll_strip(const wuss_window_t *window,
                         int                  horizontal,
                         box_t               *out)
{
  box_t titlebar;
  int   outline_px, size;

  outline_px = wuss__outline_px(window);
  size       = wuss__button_size(window);

  if (horizontal)
  {
    out->y1 = window->visible.y1 - outline_px;
    out->y0 = out->y1 - size;
    out->x0 = window->visible.x0 + outline_px;
    out->x1 = window->visible.x1 - outline_px - size; /* clear of resize corner */
  }
  else
  {
    out->x1 = window->visible.x1 - outline_px;
    out->x0 = out->x1 - size;
    wuss__titlebar_box(window, &titlebar);
    out->y0 = titlebar.y1;
    out->y1 = window->visible.y1 - outline_px - size; /* clear of resize corner */
  }
}

/* One "size"-long slice off an end of "strip" along its long axis: the near
 * end (up / left arrow) when far_end is 0, the far end (down / right arrow)
 * when far_end is 1. The cross axis spans the whole strip. */
static void scroll_end_slice(const box_t *strip,
                             int          horizontal,
                             int          far_end,
                             int          size,
                             box_t       *out)
{
  *out = *strip;

  if (horizontal)
  {
    if (far_end)
      out->x0 = strip->x1 - size;
    else
      out->x1 = strip->x0 + size;
  }
  else
  {
    if (far_end)
      out->y0 = strip->y1 - size;
    else
      out->y1 = strip->y0 + size;
  }
}

/* The well: "strip" with a "size"-long arrow slice removed from each end. */
static void scroll_middle_slice(const box_t *strip,
                                int          horizontal,
                                int          size,
                                box_t       *out)
{
  *out = *strip;

  if (horizontal)
  {
    out->x0 = strip->x0 + size;
    out->x1 = strip->x1 - size;
  }
  else
  {
    out->y0 = strip->y0 + size;
    out->y1 = strip->y1 - size;
  }
}

/* The well as a standalone call: strip, then middle slice, without the
 * caller threading "size" through. */
static void scroll_well(const wuss_window_t *window,
                        int                  horizontal,
                        box_t               *out)
{
  box_t strip;

  scroll_strip(window, horizontal, &strip);
  scroll_middle_slice(&strip, horizontal, wuss__button_size(window), out);
}

/* The sausage: a proportional handle in the well, offset by the scroll
 * position. Must track wuss__furniture_drag_sausage's maths so a drag follows
 * the pointer 1:1. */
static void scroll_sausage(const wuss_window_t *window,
                           int                  horizontal,
                           box_t               *out)
{
  box_t well, content;
  int   well_px, track_px, end_gap, content_size, doc_size;
  int   sausage_px, sausage_lo, inset, scroll;

  scroll_well(window, horizontal, &well);
  wuss__content_box(window, &content);

  if (horizontal)
  {
    well_px      = well.x1 - well.x0;
    content_size = content.x1 - content.x0;
    doc_size     = window->doc.w;
  }
  else
  {
    well_px      = well.y1 - well.y0;
    content_size = content.y1 - content.y0;
    doc_size     = window->doc.h;
  }

  /* Leave a cosmetic gap at each end of the well; drop it if the well is
   * too short to spare two gaps plus a minimum sausage. */
  end_gap  = (well_px > 2 * WUSS_SCROLL_END_GAP + WUSS_MIN_SAUSAGE) ? WUSS_SCROLL_END_GAP : 0;
  track_px = well_px - 2 * end_gap;

  if (doc_size < content_size)
    doc_size = content_size;

  sausage_px = (doc_size > 0) ? track_px * content_size / doc_size : track_px;
  if (sausage_px < WUSS_MIN_SAUSAGE)
    sausage_px = WUSS_MIN_SAUSAGE;
  if (sausage_px > track_px)
    sausage_px = track_px;

  scroll = horizontal ? window->scroll.x : window->scroll.y;

  if (doc_size > content_size && track_px > sausage_px)
    sausage_lo = end_gap + (track_px - sausage_px) * scroll / (doc_size - content_size);
  else
    sausage_lo = end_gap;

  if (horizontal)
  {
    /* Inset the sausage from the well's long edges, but only while that
     * leaves something to draw: a tiny titlebar font can make the well
     * narrower than two insets, which would invert the box. */
    inset = (well.y1 - well.y0 > 2 * WUSS_SCROLL_INSET) ? WUSS_SCROLL_INSET : 0;
    out->y0 = well.y0 + inset;
    out->y1 = well.y1 - inset;
    out->x0 = well.x0 + sausage_lo;
    out->x1 = out->x0 + sausage_px;
  }
  else
  {
    inset = (well.x1 - well.x0 > 2 * WUSS_SCROLL_INSET) ? WUSS_SCROLL_INSET : 0;
    out->x0 = well.x0 + inset;
    out->x1 = well.x1 - inset;
    out->y0 = well.y0 + sausage_lo;
    out->y1 = out->y0 + sausage_px;
  }
}

/* ----- vertical scrollbar ------------------------------------------------ */

void wuss__vscroll_up_box(const wuss_window_t *window, box_t *out)
{
  box_t strip;

  scroll_strip(window, 0, &strip);
  scroll_end_slice(&strip, 0, 0, wuss__button_size(window), out);
}

void wuss__vscroll_down_box(const wuss_window_t *window, box_t *out)
{
  box_t strip;

  scroll_strip(window, 0, &strip);
  scroll_end_slice(&strip, 0, 1, wuss__button_size(window), out);
}

void wuss__vscroll_well_box(const wuss_window_t *window, box_t *out)
{
  scroll_well(window, 0, out);
}

int wuss__vscroll_well_px(const wuss_window_t *window)
{
  box_t well;

  wuss__vscroll_well_box(window, &well);

  return well.y1 - well.y0;
}

void wuss__vscroll_sausage_box(const wuss_window_t *window, box_t *out)
{
  scroll_sausage(window, 0, out);
}

/* ----- horizontal scrollbar -------------------------------------------- */

void wuss__hscroll_left_box(const wuss_window_t *window, box_t *out)
{
  box_t strip;

  scroll_strip(window, 1, &strip);
  scroll_end_slice(&strip, 1, 0, wuss__button_size(window), out);
}

void wuss__hscroll_right_box(const wuss_window_t *window, box_t *out)
{
  box_t strip;

  scroll_strip(window, 1, &strip);
  scroll_end_slice(&strip, 1, 1, wuss__button_size(window), out);
}

void wuss__hscroll_well_box(const wuss_window_t *window, box_t *out)
{
  scroll_well(window, 1, out);
}

int wuss__hscroll_well_px(const wuss_window_t *window)
{
  box_t well;

  wuss__hscroll_well_box(window, &well);

  return well.x1 - well.x0;
}

void wuss__hscroll_sausage_box(const wuss_window_t *window, box_t *out)
{
  scroll_sausage(window, 1, out);
}
