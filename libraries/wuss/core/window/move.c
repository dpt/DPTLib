/* wuss/window/move.c -- wuss - minimal window manager */

#include "../impl.h"

/* p is the window's content top-left; the furniture offset (outline plus
 * any titlebar) is constant for a given window, so the footprint just
 * follows it. The cached furniture layout is translated in place below
 * rather than invalidated -- a pure move changes no piece's size, so there
 * is nothing to rebuild. */
void wuss_window_move(wuss_window_t *window, point_t p)
{
  box_t clean[WUSS_MAX_INVALIDATE_PIECES];
  box_t full_dest[WUSS_MAX_INVALIDATE_PIECES];
  box_t copied[WUSS_MAX_INVALIDATE_PIECES];
  int   width, height, outline_px, titlebar_height;
  int   dx, dy, nclean, ncopied, i;
  box_t before, dirty;

  width           = window->visible.x1 - window->visible.x0;
  height          = window->visible.y1 - window->visible.y0;
  outline_px      = wuss__outline_px(window);
  titlebar_height = wuss__titlebar_height(window);
  before          = window->visible;

  /* a drag delivers one call per pointer-move event, not one per actual
   * change of position -- a window pinned against the screen edge, or a
   * backend that reports redundant motion, can call this with the same
   * target it's already at. width/height never change here, so an
   * unchanged origin means an unchanged box; skip the packer release,
   * layout translate and blit/invalidate machinery entirely. */
  if (p.x - outline_px == before.x0 &&
      p.y - outline_px - titlebar_height == before.y0)
    return;

  /* a manual move desyncs the window from its layout-packer slot; hand the
   * slot back and stop tracking this window's position */
  wuss__release_packed(window);

  /* a hidden window has nothing on screen to slide and must paint nothing;
   * just translate its footprint so it is in place when shown again */
  if (window->flags & wuss_WINDOW_HIDDEN)
  {
    /* nothing is drawn from the cache while hidden, but the next unhide must
     * not paint it back at the old position, so just drop it -- there's no
     * screen-visible dirty region to queue either way */
    wuss__chrome_invalidate_layout(window);

    window->visible.x0 = p.x - outline_px;
    window->visible.y0 = p.y - outline_px - titlebar_height;
    window->visible.x1 = window->visible.x0 + width;
    window->visible.y1 = window->visible.y0 + height;
    wuss__notify_open(window);
    return;
  }

  /* The clean (non-occluded) pieces of "before" are genuinely this
   * window's own rendering; whatever isn't clean is hidden behind some
   * other window and has no valid pixels of this window's content to
   * slide. Computed against the current z-order, before the move. */
  nclean = wuss__clip_to_visible(window, &before, clean);

  /* A piece can also be "clean" by occlusion yet still not show valid
   * pixels on screen: an earlier move (or any other invalidation) this same
   * frame may have queued part of it in wuss->dirty[] without wuss_redraw_
   * dirty having run yet to actually repaint it -- e.g. several batched
   * pointer-move events arriving before the next redraw, which pushes a
   * window on and off the screen edge repeatedly. Sliding that stale ground
   * would just paste it, untouched, onto the window's new position. Strip
   * every pending-dirty region out of "clean" first so only pixels already
   * settled on screen are treated as a valid blit source. */
  nclean = wuss__filter_settled(clean, nclean, window->wuss->dirty,
                                window->wuss->ndirty);

  window->visible.x0 = p.x - outline_px;
  window->visible.y0 = p.y - outline_px - titlebar_height;
  window->visible.x1 = window->visible.x0 + width;
  window->visible.y1 = window->visible.y0 + height;

  wuss__notify_open(window);

  dx = window->visible.x0 - before.x0;
  dy = window->visible.y0 - before.y0;

  /* a pure translation leaves every cached rect correct relative to the
   * window, just offset in screen space -- shift it instead of dropping the
   * cache and rebuilding all pieces on the next paint of even a thin
   * sliver */
  if (window->furniture_layout.flags & wuss_FURNITURE_LAYOUT__VALID)
    wuss__furniture_layout_translate(window, dx, dy);

  for (i = 0; i < nclean; i++)
    box_translated(&clean[i], dx, dy, &full_dest[i]);

  /* Slide the clean pieces by (dx, dy); wuss__blit_pieces clips each
   * destination clear of the windows above this one -- an occluder there
   * hasn't moved, so its pixels are already correct and pasting stale ones
   * over them would just have to be repainted straight back. It also
   * repairs any piece that slid partly off-screen. */
  if (nclean > 0 &&
      wuss__blit_pieces(window, clean, nclean, dx, dy, NULL, copied, &ncopied))
  {
    box_t hidden[WUSS_MAX_INVALIDATE_PIECES];
    int   nhidden;

    /* Each clean piece is, by construction, clear of any occluder at its
     * old position, so the vacated sliver left behind by sliding it to its
     * full (untrimmed) new position is safe to invalidate raw, without
     * re-checking occlusion -- regardless of whether every pixel of that
     * new position actually got a blit above: the part that landed under an
     * occluder was skipped there, but the old position is vacated either
     * way.
     *
     * The sliver is clean[i] minus *every* clean piece's destination, not
     * just its own: with an occluder biting a corner out of "before", one
     * clean piece can slide onto ground another clean piece just vacated
     * (e.g. a full-width bottom band vacated straight into the destination
     * of the right-side band on a downward drag). That overlap already got
     * valid pixels from the other piece's blit, so invalidating it would
     * just repaint good pixels. */
    for (i = 0; i < nclean; i++)
    {
      box_t sliver[WUSS_MAX_INVALIDATE_PIECES];
      int   nsliver, s;

      /* on overflow wuss__subtract_boxes falls back to clean[i] whole
       * (over-invalidating, not dropping it) -- see carve_by_cuts. */
      nsliver = wuss__subtract_boxes(&clean[i], full_dest, nclean, sliver);
      for (s = 0; s < nsliver; s++)
        wuss_invalidate(window->wuss, &sliver[s]);
    }

    /* Whatever of "before" wasn't clean has no valid source pixels: its
     * translated destination needs a genuine repaint, clipped against
     * whatever's above this window there now. */
    nhidden = wuss__subtract_boxes(&before, clean, nclean, hidden);
    for (i = 0; i < nhidden; i++)
    {
      box_t hidden_dest;

      box_translated(&hidden[i], dx, dy, &hidden_dest);
      wuss__invalidate_clipped(window, &hidden_dest);
    }
  }
  else
  {
    /* Nothing of "before" was clean, splitting overflowed the piece budget,
     * the pieces would have clobbered each other, or the blit was declined:
     * fall back to a normal clipped redraw of the whole moved footprint. */
    logf_warning("wuss_window_move: blit fast path failed, repainting the "
                 "whole footprint (nclean=%d)", nclean);
    box_union(&before, &window->visible, &dirty);
    wuss__invalidate_clipped(window, &dirty);
  }

  (void) ncopied; /* move.c repairs via the sliver logic above, not "copied" */
}
