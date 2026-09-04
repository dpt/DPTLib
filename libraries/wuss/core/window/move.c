/* wuss/window/move.c -- wuss - minimal window manager */

#include "../impl.h"

/* p is the window's content top-left; the furniture offset (outline plus
 * any titlebar) is constant for a given window, so the footprint just
 * follows it */
void wuss_window_move(wuss_window_t *window, point_t p)
{
  box_t   clean[WUSS_MAX_INVALIDATE_PIECES];
  box_t   full_dest[WUSS_MAX_INVALIDATE_PIECES];
  box_t   copied[WUSS_MAX_INVALIDATE_PIECES];
  int     width, height, outline_px, titlebar_height;
  int     dx, dy, nclean, ncopied, i;
  box_t   before, dirty;

  /* a manual move desyncs the window from its layout-packer slot; hand the
   * slot back and stop tracking this window's position */
  wuss__release_packed(window);

  width           = window->visible.x1 - window->visible.x0;
  height          = window->visible.y1 - window->visible.y0;
  outline_px      = wuss__outline_px(window);
  titlebar_height = wuss__titlebar_height(window);
  before          = window->visible;

  /* a hidden window has nothing on screen to slide and must paint nothing;
   * just translate its footprint so it is in place when shown again */
  if (window->flags & wuss_WINDOW_HIDDEN)
  {
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

  window->visible.x0 = p.x - outline_px;
  window->visible.y0 = p.y - outline_px - titlebar_height;
  window->visible.x1 = window->visible.x0 + width;
  window->visible.y1 = window->visible.y0 + height;

  wuss__notify_open(window);

  dx = window->visible.x0 - before.x0;
  dy = window->visible.y0 - before.y0;

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

      /* ponytail: nclean is a handful, so this can't approach the
       * WUSS_MAX_INVALIDATE_PIECES cap; if that ever changes, a dropped
       * piece here means a missed repaint (visible corruption), not just
       * wasted work -- revisit then. */
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
    box_union(&before, &window->visible, &dirty);
    wuss__invalidate_clipped(window, &dirty);
  }

  (void) ncopied; /* move.c repairs via the sliver logic above, not "copied" */
}
