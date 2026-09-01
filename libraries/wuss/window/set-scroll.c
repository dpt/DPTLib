/* wuss/window/set-scroll.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss_window_set_scroll(wuss_window_t *window, point_t p)
{
  box_t content;
  box_t src[WUSS_MAX_INVALIDATE_PIECES];
  box_t copied[WUSS_MAX_INVALIDATE_PIECES];
  box_t dirty[WUSS_MAX_INVALIDATE_PIECES];
  box_t vis[WUSS_MAX_INVALIDATE_PIECES];
  int   dx, dy, nsrc, ncopied, ndirty, nvis, i, j;

  dx = p.x - window->scroll.x;
  dy = p.y - window->scroll.y;
  if (dx == 0 && dy == 0)
    return;

  window->scroll = p;

  wuss__content_box(window, &content);

#ifdef WUSS_FURNITURE
  /* the scrollbar sausage position depends on scroll, so its well needs
   * redrawing too -- content invalidation alone never touches it */
  window->wuss->furniture_ops->invalidate(window);
#endif

  /* The valid blit source is the content box minus whatever windows above it
   * were covering (those areas hold occluder pixels, not this window's).
   * Sliding a source piece by the scroll delta can still land its
   * destination on screen a higher window owns -- screen_copy_rect only
   * clips to the content box, so it would paint over the occluder there.
   * Clip each destination against the occluders too and blit only the
   * surviving sub-pieces, each with its own matching source offset; the
   * rest falls into the repaint set below. Falls back to a full content
   * invalidate when the pixel format can't blit. */
  nsrc              = wuss__clip_to_visible(window, &content, src);
  ncopied           = 0;
  window->wuss->scr->clip = content;
  for (i = 0; i < nsrc; i++)
  {
    box_t want;

    box_translated(&src[i], -dx, -dy, &want);
    nvis = wuss__clip_to_visible(window, &want, vis);
    for (j = 0; j < nvis; j++)
    {
      box_t ssub, got;

      box_translated(&vis[j], dx, dy, &ssub); /* source for this dest piece */
      if (screen_copy_rect(window->wuss->scr, &ssub,
                           POINT(ssub.x0 - dx, ssub.y0 - dy), &got) != 0 &&
          ncopied < WUSS_MAX_INVALIDATE_PIECES)
        copied[ncopied++] = got;
    }
  }

  if (ncopied > 0)
  {
    /* Repaint the content box minus what the blit reused, then clip each
     * survivor to this window's visible area: the parts that were behind an
     * occluder still show the occluder's own correct pixels, so leaving
     * them out keeps the scroll from redrawing the occluding window. */
    ndirty = wuss__subtract_boxes(&content, copied, ncopied, dirty);
    for (i = 0; i < ndirty; i++)
      wuss__invalidate_clipped(window, &dirty[i]);
    return;
  }

  wuss__invalidate_clipped(window, &content);
}
