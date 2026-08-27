/* move.c -- wuss - minimal window manager */

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
  box_t   dest[WUSS_MAX_INVALIDATE_PIECES];
  int     width, height, outline_px, titlebar_height;
  int     dx, dy, nclean, i;
  box_t   before, dirty, copied;
  int     blit_failed;

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

  blit_failed = 0;
  for (i = 0; i < nclean; i++)
  {
    translate_box(&clean[i], dx, dy, &dest[i]);

    if (!screen_copy_rect(window->wuss->scr, &clean[i],
                          (point_t) { dest[i].x0, dest[i].y0 }, &copied))
    {
      /* The screen format doesn't support the blit at all (e.g. paletted):
       * this fails identically for every piece, so bail out before any
       * blits have happened rather than leaving a half-blitted window. */
      blit_failed = 1;
      break;
    }

    /* Each clean piece is, by construction, clear of any occluder at its
     * old position, so the vacated sliver left behind by sliding it to
     * "dest[i]" is safe to invalidate raw, without re-checking occlusion. */
    wuss__invalidate_minus(window->wuss, &clean[i], &dest[i]);

    /* "copied" can be smaller than "dest[i]" if the move was partly
     * off-screen: the leftover part has no valid source pixels behind it,
     * so it needs a real repaint too. */
    wuss__invalidate_minus(window->wuss, &dest[i], &copied);
  }

  if (nclean > 0 && !blit_failed)
  {
    box_t hidden[WUSS_MAX_INVALIDATE_PIECES];
    int   nhidden;

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

    /* Each blit is a raw pixel copy: if a clean piece's destination lands
     * under a window above this one in z-order, it just pasted this
     * window's stale pixels straight over that occluder's rendering. Force
     * exactly that overlap dirty (unclipped, so the redraw picks up the
     * occluder as the topmost owner there) to repair it. */
    wuss__invalidate_uncovered(window);
  }
  else
  {
    /* Nothing of "before" was clean, or the blit was declined: fall back
     * to a normal clipped redraw of the whole moved footprint. */
    box_union(&before, &window->visible, &dirty);
    wuss__invalidate_clipped(window, &dirty);
  }
}
