/* wuss/window/set-scroll.c -- wuss - minimal window manager */

#include "../impl.h"

void wuss_window_set_scroll(wuss_window_t *window, point_t p)
{
  box_t content;
  box_t src[WUSS_MAX_INVALIDATE_PIECES];
  box_t copied[WUSS_MAX_INVALIDATE_PIECES];
  box_t dirty[WUSS_MAX_INVALIDATE_PIECES];
  int   dx, dy, nsrc, ncopied, ndirty, i;

  dx = p.x - window->scroll.x;
  dy = p.y - window->scroll.y;
  if (dx == 0 && dy == 0)
    return;

  window->scroll = p;

  wuss__content_box(window, &content);

#ifdef WUSS_FURNITURE
  /* the scrollbar sausage position depends on scroll, so its well needs
   * redrawing too -- content invalidation alone never touches it */
  wuss__furniture_invalidate(window);
#endif

  /* The valid blit source is the content box minus whatever windows above it
   * were covering (those areas hold occluder pixels, not this window's), so
   * slide it piece by piece by the scroll delta. The clip keeps every
   * destination inside the content box, so a piece near the trailing edge
   * shifts partly out and that area falls into the repaint set below along
   * with the occluded regions. Falls back to a full content invalidate when
   * the pixel format can't blit. */
  nsrc              = wuss__clip_to_visible(window, &content, src);
  ncopied           = 0;
  window->wuss->scr->clip = content;
  for (i = 0; i < nsrc; i++)
  {
    box_t got;

    if (screen_copy_rect(window->wuss->scr, &src[i],
                         POINT(src[i].x0 - dx, src[i].y0 - dy), &got) != 0 &&
        ncopied < WUSS_MAX_INVALIDATE_PIECES)
      copied[ncopied++] = got;
  }

  if (ncopied > 0)
  {
    ndirty = wuss__subtract_boxes(&content, copied, ncopied, dirty);
    for (i = 0; i < ndirty; i++)
      wuss_invalidate(window->wuss, &dirty[i]);
    return;
  }

  wuss__invalidate_clipped(window, &content);
}
