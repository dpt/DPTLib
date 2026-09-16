/* wuss/window/resize.c -- wuss - minimal window manager */

#include "base/utils.h"
#include "geom/box.h"

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
  point_t old_scroll;
  box_t   before_content;
  point_t carve;
  point_t clamped;

  if (!wuss__size_ok(size.w, size.h))
    return result_WUSS_TOO_SMALL;

  /* The requested content size is honoured verbatim: a window may end up
   * overhanging the screen edge (same as one dragged there, or one whose
   * far corner is off-screen when toggle-size floors it at WUSS_MIN_CONTENT).
   * Only wuss_window_create still caps at the screen. */

  outline_px      = wuss__outline_px(window);
  titlebar_height = wuss__titlebar_height(window);
  before          = window->visible;
  old_scroll      = window->scroll;
  wuss__content_box(window, &before_content);
  wuss__furniture_carve_for(window->flags, wuss__button_size(window), &carve);

  /* a resize drag delivers one call per pointer-move event, not one per
   * actual change of size -- a window pinned against the screen edge or
   * the minimum-size clamp can call this repeatedly at the size it's
   * already at. The top-left corner never moves here, so an unchanged
   * bottom-right corner means an unchanged box; skip the packer release
   * and the invalidate/blit machinery entirely. */
  if (window->visible.x0 + size.w + 2 * outline_px + carve.x == before.x1 &&
      window->visible.y0 + size.h + titlebar_height + 2 * outline_px + carve.y == before.y1)
    return result_OK;

  /* a manual resize desyncs the window from its layout-packer slot */
  wuss__release_packed(window);

  window->visible.x1 = window->visible.x0 + size.w + 2 * outline_px + carve.x;
  window->visible.y1 = window->visible.y0 + size.h + titlebar_height + 2 * outline_px + carve.y;

  wuss__notify_open(window);

  /* Enlarging the viewport (or growing it past the doc extent) can scroll
   * content that no longer exists into view; pull the offset back so only
   * the document extent is ever shown. The content pixels already on screen
   * are still this window's own rendering, just at the old scroll offset --
   * so slide them by the scroll delta and only the newly-exposed strip(s)
   * need a real repaint. The valid source is the old content box minus
   * whatever windows above it were covering (those areas hold occluder
   * pixels, not this window's), so blit it piece by piece; the clip bounds
   * every destination to the new content box, discarding anything sliding
   * off the old box's far edge rather than dragging stale pixels in. If the
   * screen format can't blit, fall back to repainting the whole content
   * box. Either way the grown/shrunk-sliver logic below still assumes an
   * unchanged interior, which now holds. */
  clamped = wuss__scroll_clamp(window, window->scroll);
  if (clamped.x != window->scroll.x || clamped.y != window->scroll.y)
  {
    box_t content;
    box_t stale[WUSS_MAX_DIRTY];
    box_t clean[WUSS_MAX_INVALIDATE_PIECES];
    box_t copied[WUSS_MAX_INVALIDATE_PIECES];
    box_t dirty[WUSS_MAX_INVALIDATE_PIECES];
    int   dx, dy, nstale, nclean, nsrc, ncopied, ndirty, i, overflow;

    dx = clamped.x - old_scroll.x;
    dy = clamped.y - old_scroll.y;
    window->scroll = clamped;
    wuss__content_box(window, &content);

    /* A resize drag delivers several calls per frame, same as a fast wheel
     * spin (see wuss_window_set_scroll): an earlier call this frame may have
     * queued part of "before_content" in wuss->dirty[] without a redraw
     * having painted it yet. Sliding that stale ground would smear garbage
     * into the window instead of this window's own settled pixels. */
    nstale = 0;
    for (i = 0; i < window->wuss->ndirty; i++)
      if (box_intersects(&window->wuss->dirty[i], &before_content))
        stale[nstale++] = window->wuss->dirty[i];

    /* Each subtract can split a piece into up to four bands, so this can
     * overflow the piece budget on a badly fragmented window -- treat that
     * as "no safe fast path" (see wuss_window_set_scroll) rather than
     * silently dropping survivors: a dropped clean piece never gets
     * invalidated either, since the invalidate below only covers "content
     * minus copied", not "content minus every clean piece found" -- so a
     * silently-dropped piece would leave genuinely stale pre-resize pixels
     * on screen with nothing left to repaint them. wuss__filter_settled caps
     * at WUSS_MAX_INVALIDATE_PIECES the same way, so treat a full result as
     * a possible overflow too. */
    nclean            = wuss__clip_to_visible(window, &before_content, clean);
    nsrc              = wuss__filter_settled(clean, nclean, stale, nstale);
    overflow          = (nsrc == WUSS_MAX_INVALIDATE_PIECES);

    ncopied           = 0;
    if (!overflow)
    {
      window->wuss->scr->clip = content;
      for (i = 0; i < nsrc; i++)
      {
        box_t got;

        if (screen_copy_rect(window->wuss->scr, &clean[i],
                             POINT(clean[i].x0 - dx, clean[i].y0 - dy),
                             &got) == result_OK)
        {
          /* "got" is already correct on screen -- the blit reused it --
           * but a frontend re-uploading only wuss_get_touched_extent still
           * needs to know it moved. */
          wuss__touch(window->wuss, &got);

          if (ncopied < WUSS_MAX_INVALIDATE_PIECES)
            copied[ncopied++] = got;
        }
      }
    }

    if (ncopied > 0)
    {
      /* Repaint the content box minus what the blit reused, each survivor
       * clipped to this window's visible area: parts that were behind an
       * occluder still show the occluder's own correct pixels, so leaving
       * them out keeps the resize from redrawing the occluding window. */
      ndirty = wuss__subtract_boxes(&content, copied, ncopied, dirty);
      for (i = 0; i < ndirty; i++)
        wuss__invalidate_clipped(window, &dirty[i]);
    }
    else
    {
      /* the scroll-reclamp blit copied nothing (paletted screen, every
       * piece off-screen, or the piece budget overflowed): repaint the
       * whole content box */
      logf_warning("wuss_window_resize: scroll-reclamp blit copied nothing, "
                   "repainting the whole content box (nsrc=%d, overflow=%d)",
                   nsrc, overflow);
      wuss__invalidate_clipped(window, &content);
    }
  }

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

    /* the dirty box above covers every furniture strip, but nothing here
     * has dropped the cached furniture layout -- it still holds the old
     * width's rects. Force a rebuild so the redraw paints furniture at the
     * new geometry (the else branch gets this via wuss__chrome_repaint). */
    wuss__chrome_invalidate_layout(window);
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
      wuss__chrome_repaint_for(window, &before);
    wuss__chrome_repaint(window);
  }

  return result_OK;
}
