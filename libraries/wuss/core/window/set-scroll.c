/* wuss/window/set-scroll.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss_window_set_scroll(wuss_window_t *window, point_t p)
{
  box_t content;
  box_t stale[WUSS_MAX_DIRTY];
  box_t clean[WUSS_MAX_INVALIDATE_PIECES];
  box_t src[WUSS_MAX_INVALIDATE_PIECES];
  box_t copied[WUSS_MAX_INVALIDATE_PIECES];
  box_t dirty[WUSS_MAX_INVALIDATE_PIECES];
  int   dx, dy, nstale, nclean, nsrc, ncopied, ndirty, i, overflow;

  dx = p.x - window->scroll.x;
  dy = p.y - window->scroll.y;
  if (dx == 0 && dy == 0)
    return;

  window->scroll = p;

  wuss__content_box(window, &content);

  /* Any content-box area already sitting in the dirty list holds stale pixels
   * (e.g. the strip exposed by an earlier scroll this frame, not yet
   * redrawn): a wheel spin can deliver several scrolls before wuss_redraw_dirty
   * runs, and the blit below is a framebuffer memmove -- feeding it a stale
   * source would smear that stale content into the interior. Collect those
   * regions now, before the furniture invalidate adds its own unrelated
   * rects, and exclude them from the blit source further down. */
  nstale = 0;
  for (i = 0; i < window->wuss->ndirty; i++)
    if (box_intersects(&window->wuss->dirty[i], &content))
      stale[nstale++] = window->wuss->dirty[i];

#ifdef WUSS_FURNITURE
  /* the scrollbar sausage position depends on scroll, so its well needs
   * redrawing too -- content invalidation alone never touches it */
  window->wuss->furniture_ops->invalidate(window);
#endif

  /* The valid blit source is the content box minus whatever windows above it
   * were covering (those areas hold occluder pixels, not this window's),
   * minus the stale regions collected above. wuss__blit_pieces then clips
   * each destination against the occluders too. Each subtract can split a
   * piece into up to four bands, so this can overflow the piece budget on a
   * badly fragmented window -- treat that as "no safe fast path". */
  nclean   = wuss__clip_to_visible(window, &content, clean);
  nsrc     = 0;
  overflow = 0;
  for (i = 0; i < nclean; i++)
  {
    box_t kept[WUSS_MAX_INVALIDATE_PIECES];
    int   nkept, k;

    nkept = wuss__subtract_boxes(&clean[i], stale, nstale, kept);
    for (k = 0; k < nkept; k++)
    {
      if (nsrc == WUSS_MAX_INVALIDATE_PIECES)
      {
        overflow = 1;
        break;
      }
      src[nsrc++] = kept[k];
    }
    if (overflow)
      break;
  }

  if (!overflow &&
      wuss__blit_pieces(window, src, nsrc, -dx, -dy, &content,
                        copied, &ncopied))
  {
    /* Repaint the content box minus what the blit reused, each survivor
     * clipped to this window's visible area: parts that were behind an
     * occluder still show the occluder's own correct pixels, so leaving
     * them out keeps the scroll from redrawing the occluding window. */
    ndirty = wuss__subtract_boxes(&content, copied, ncopied, dirty);
    for (i = 0; i < ndirty; i++)
      wuss__invalidate_clipped(window, &dirty[i]);
    return;
  }

  wuss__invalidate_clipped(window, &content);
}
