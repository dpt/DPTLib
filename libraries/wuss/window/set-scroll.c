/* wuss/window/set-scroll.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss_window_set_scroll(wuss_window_t *window, point_t p)
{
  box_t content;
  box_t src[WUSS_MAX_INVALIDATE_PIECES];
  box_t blit_src[WUSS_MAX_INVALIDATE_PIECES];
  box_t blit_dest[WUSS_MAX_INVALIDATE_PIECES];
  box_t copied[WUSS_MAX_INVALIDATE_PIECES];
  box_t dirty[WUSS_MAX_INVALIDATE_PIECES];
  box_t vis[WUSS_MAX_INVALIDATE_PIECES];
  int   order[WUSS_MAX_INVALIDATE_PIECES];
  int   dx, dy, nsrc, nblit, ncopied, ndirty, nvis, i, j, idx;
  int   overflow, blit_failed;

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
   * Clip each destination against the occluders too and keep only the
   * surviving sub-pieces, each with its own matching source offset. */
  nsrc     = wuss__clip_to_visible(window, &content, src);
  nblit    = 0;
  overflow = 0;
  for (i = 0; i < nsrc && !overflow; i++)
  {
    box_t want;

    box_translated(&src[i], -dx, -dy, &want);
    nvis = wuss__clip_to_visible(window, &want, vis);
    for (j = 0; j < nvis; j++)
    {
      if (nblit == WUSS_MAX_INVALIDATE_PIECES)
      {
        overflow = 1;
        break;
      }
      blit_dest[nblit] = vis[j];
      box_translated(&vis[j], dx, dy, &blit_src[nblit]);
      nblit++;
    }
  }

  /* Each sub-piece blit is a self-consistent memmove, but one piece's
   * destination can land on another piece's still-unread source (e.g. the
   * bands carved around a floating occluder overlap once shifted). Order
   * them so that never happens; fall back to a full content invalidate if
   * no safe order exists or the piece budget overflowed. */
  window->wuss->scr->clip = content;
  ncopied     = 0;
  blit_failed = nsrc == 0 || overflow ||
               !wuss__order_pieces(blit_src, blit_dest, nblit, order);

  for (i = 0; i < nblit && !blit_failed; i++)
  {
    box_t got;

    idx = order[i];
    if (screen_copy_rect(window->wuss->scr, &blit_src[idx],
                         POINT(blit_dest[idx].x0, blit_dest[idx].y0),
                         &got) == 0)
    {
      blit_failed = 1;
      break;
    }
    if (ncopied < WUSS_MAX_INVALIDATE_PIECES)
      copied[ncopied++] = got;
  }

  if (!blit_failed && ncopied > 0)
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
