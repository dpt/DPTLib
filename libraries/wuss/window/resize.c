/* resize.c -- wuss - minimal window manager */

#include "base/utils.h"

#include "../impl.h"

/* Invalidate the part of "a" not covered by "b", given both share the same
 * top-left corner (true of a window's visible box before/after a resize, as
 * only the bottom-right corner moves): the difference splits into exactly
 * two non-overlapping rectangles, each clipped against occluders before
 * queueing. */
static void invalidate_grown_or_shrunk(wuss_window_t *window,
                                       const box_t   *a,
                                       const box_t   *b)
{
  box_t piece;

  if (a->x1 > b->x1)
  {
    piece.x0 = b->x1; piece.y0 = a->y0;
    piece.x1 = a->x1; piece.y1 = a->y1;
    wuss__invalidate_clipped(window, &piece);
  }

  if (a->y1 > b->y1)
  {
    piece.x0 = a->x0;            piece.y0 = b->y1;
    piece.x1 = MIN(a->x1, b->x1); piece.y1 = a->y1;
    wuss__invalidate_clipped(window, &piece);
  }
}

result_t wuss_window_resize(wuss_window_t *window, size2d_t size)
{
  int     outline_px, titlebar_height;
  box_t   before;
  point_t carve;

  if (!wuss__size_ok(size.w, size.h))
    return result_WUSS_TOO_SMALL;

  /* a manual resize desyncs the window from its layout-packer slot */
  wuss__release_packed(window);

  outline_px      = wuss__outline_px(window);
  titlebar_height = wuss__titlebar_height(window);
  before          = window->visible;
  wuss__furniture_carve_for(window->flags, wuss__button_size(window), &carve);

  window->visible.x1 = window->visible.x0 + size.w + 2 * outline_px + carve.x;
  window->visible.y1 = window->visible.y0 + size.h + titlebar_height + 2 * outline_px + carve.y;

  wuss__notify_open(window);

  if (window->flags & wuss_WINDOW_NO_RESIZE_BLIT)
  {
    /* This task's content isn't just anchored positions plus furniture --
     * it lays itself out across the whole window (e.g. a palette swatch
     * grid), so the "unchanged interior" assumption below doesn't hold:
     * every pixel of the new footprint needs redrawing, not just the
     * grown/shrunk sliver. */
    box_t dirty;

    box_union(&before, &window->visible, &dirty);
    wuss__invalidate_clipped(window, &dirty);
  }
  else
  {
    /* The top-left corner is fixed, so any pixels within both the old and
     * new footprint are unchanged and don't need repainting -- only the
     * shrunk-away sliver (revealing whatever is now behind it) and the
     * grown-into sliver (this window's own previously-undrawn content) do. */
    invalidate_grown_or_shrunk(window, &before, &window->visible);
    invalidate_grown_or_shrunk(window, &window->visible, &before);

    /* Growing strands old furniture (e.g. the old outline/scrollbar edge)
     * inside what's now interior content -- a region both calls above treat
     * as already-valid and so never repaint. Force its old position dirty
     * too. Shrinking needs no such help: old furniture positions only ever
     * land outside the new, smaller box, already covered above. */
    if (window->visible.x1 - window->visible.x0 > before.x1 - before.x0 ||
        window->visible.y1 - window->visible.y0 > before.y1 - before.y0)
      wuss__furniture_invalidate_for(window, &before);
    wuss__furniture_invalidate(window);
  }

  return result_OK;
}
