/* wuss/window/move.c -- wuss - minimal window manager */

#include "../impl.h"

/* Translate "box" by (dx, dy) into "out". */
static void translate_box(const box_t *box, int dx, int dy, box_t *out)
{
  out->x0 = box->x0 + dx;
  out->y0 = box->y0 + dy;
  out->x1 = box->x1 + dx;
  out->y1 = box->y1 + dy;
}

/* p is the window's content top-left; the furniture offset (outline plus
 * any titlebar) is constant for a given window, so the footprint just
 * follows it */
void wuss_window_move(wuss_window_t *window, point_t p)
{
  box_t   clean[WUSS_MAX_INVALIDATE_PIECES];
  box_t   full_dest[WUSS_MAX_INVALIDATE_PIECES];
  box_t   blit_src[WUSS_MAX_INVALIDATE_PIECES];
  box_t   blit_dest[WUSS_MAX_INVALIDATE_PIECES];
  int     order[WUSS_MAX_INVALIDATE_PIECES];
  int     width, height, outline_px, titlebar_height;
  int     dx, dy, nclean, nblit, overflow, i, idx;
  box_t   before, dirty, copied;
  int     blit_failed;

  /* a manual move desyncs the window from its layout-packer slot; hand the
   * slot back and stop tracking this window's position */
  wuss__release_packed(window);

  width           = window->visible.x1 - window->visible.x0;
  height          = window->visible.y1 - window->visible.y0;
  outline_px      = wuss__outline_px(window);
  titlebar_height = wuss__titlebar_height(window);
  before          = window->visible;

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
    translate_box(&clean[i], dx, dy, &full_dest[i]);

  /* Split each clean piece's full (untrimmed) destination down to the parts
   * not already sitting under an occluder above this window there: that
   * occluder hasn't moved, so its pixels are already correct, and blitting
   * this window's stale pixels over them would just have to be repainted
   * straight back -- cheaper to never touch them at all. Only the surviving,
   * genuinely-blittable sub-pieces go on to the clobber-ordering/blit below;
   * the occluded remainder needs no repair because nothing was ever pasted
   * over it. */
  nblit    = 0;
  overflow = 0;
  for (i = 0; i < nclean && !overflow; i++)
  {
    box_t visible_dest[WUSS_MAX_INVALIDATE_PIECES];
    int   nvisible, v;

    nvisible = wuss__clip_to_visible(window, &full_dest[i], visible_dest);

    for (v = 0; v < nvisible; v++)
    {
      if (nblit == WUSS_MAX_INVALIDATE_PIECES)
      {
        overflow = 1;
        break;
      }

      blit_dest[nblit] = visible_dest[v];
      translate_box(&visible_dest[v], -dx, -dy, &blit_src[nblit]);
      nblit++;
    }
  }

  /* If no ordering of these single-rect blits avoids one clobbering
   * another's still-unread source, fall back rather than risk corrupting
   * this window's own pixels. */
  blit_failed = nclean == 0 || overflow ||
               !wuss__order_pieces(blit_src, blit_dest, nblit, order);

  for (i = 0; i < nblit && !blit_failed; i++)
  {
    idx = order[i];

    if (!screen_copy_rect(window->wuss->scr, &blit_src[idx],
                          POINT(blit_dest[idx].x0, blit_dest[idx].y0),
                          &copied))
    {
      /* screen_copy_rect refused this piece: either the screen format has no
       * blit path (e.g. paletted -- fails on the first piece, before anything
       * has moved) or this piece's source/dest lies off-screen, which can
       * happen part-way through after earlier pieces have already blitted.
       * Either way, bail: the pieces done so far are self-consistent and the
       * caller's fallback full invalidate repaints the whole union. */
      blit_failed = 1;
      break;
    }

    /* "copied" can be smaller than "blit_dest[idx]" if the move was partly
     * off-screen: the leftover part has no valid source pixels behind it,
     * so it needs a real repaint too. Safe to invalidate raw, without
     * re-checking occlusion, since "blit_dest[idx]" (and so its "copied"
     * subset) was already clipped clear of every occluder above. */
    wuss__invalidate_minus(window->wuss, &blit_dest[idx], &copied);
  }

  if (nclean > 0 && !blit_failed)
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

      translate_box(&hidden[i], dx, dy, &hidden_dest);
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
}
