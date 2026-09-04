/* wuss/redraw.c -- wuss - minimal window manager */

#include "impl.h"

static void redraw_window(wuss_t        *wuss,
                          wuss_window_t *win,
                          const box_t   *full,
                          result_t      *rc)
{
  box_t clipped;
  box_t visible_clipped;
  box_t content;
  box_t pieces[WUSS_MAX_INVALIDATE_PIECES];
  int   npieces, i;

  if (win->flags & wuss_WINDOW_HIDDEN)
    return; /* not drawn while hidden */

  if (box_intersection(&win->visible, full, &visible_clipped))
    return; /* offscreen */

  wuss__chrome_draw(wuss, win, full);

  wuss__content_box(win, &content);
  if (box_intersection(&content, full, &clipped))
    return;

  /* skip (or shrink to) whatever part of "clipped" isn't hidden behind a
   * higher window -- otherwise every dirty rect raised by a window on top
   * (e.g. a moving ball) would also repaint whatever it's covering below,
   * however expensive that redraw is, for pixels nobody will ever see */
  npieces = wuss__clip_to_visible(win, &clipped, pieces);

  for (i = 0; i < npieces; i++)
  {
    wuss->scr->clip = pieces[i];

    /* phase any pattern against the scroll origin so it locks to the
     * content rather than crawling as the window scrolls */
    wuss__fill_backdrop(wuss->scr, wuss->palette, &win->bg, &content,
                        content.x0 - win->scroll.x,
                        content.y0 - win->scroll.y);

    {
      wuss_event_t event;
      result_t     crc;

      event.kind                 = wuss_EVENT_REDRAW;
      event.data.redraw.scr      = wuss->scr;
      event.data.redraw.content  = &pieces[i];
      event.data.redraw.bounds   = &content;
      event.data.redraw.scroll = win->scroll;
      crc = wuss__deliver(win->task, win, &event);
      if (crc != result_OK)
        *rc = crc;
    }
    
#ifdef WUSS_ICONS
    {
      int k;

      /* draw in array order so later-created icons paint on top, matching
       * wuss__icon_hit_test's reverse scan */
      for (k = 0; k < win->nicons; k++)
        wuss__icon_draw(wuss, win->icons[k], &content, win->scroll);
    }
#endif
  }
}

/* draws back-to-front (list head = topmost window drawn last) without
 * recursing per window, so stack use stays flat regardless of window count;
 * ponytail: O(n^2) walk, fine while window counts stay small, switch to an
 * array/vector pass if that stops being true */
static void redraw_from(wuss_t      *wuss,
                        list_t      *head,
                        const box_t *full,
                        result_t    *rc)
{
  const list_t *stop;

  stop = NULL;
  for (;;)
  {
    list_t *e, *last;

    last = NULL;
    for (e = head; e != stop; e = e->next)
      last = e;

    if (last == NULL)
      break;

    redraw_window(wuss, wuss__window_from_link(last), full, rc);
    stop = last;
  }
}

result_t wuss_redraw(wuss_t *wuss)
{
  box_t    full;
  result_t rc;

  full.x0 = 0;
  full.y0 = 0;
  full.x1 = wuss->scr->size.w;
  full.y1 = wuss->scr->size.h;

  wuss->scr->clip = full;
  wuss__fill_backdrop(wuss->scr, wuss->palette, &wuss->backdrop, &full, 0, 0);

  rc = result_OK;
  redraw_from(wuss, wuss->z_order.next, &full, &rc);

  /* redraw_window narrows wuss->scr->clip to whatever it last painted;
   * reset it so anything drawing after this redraw (not least
   * screen_copy_rect, used for window-drag blitting) sees the whole
   * screen rather than that leftover sliver. */
  box_reset(&wuss->scr->clip);

  wuss->ndirty = 0;

  return rc;
}

result_t wuss_redraw_dirty(wuss_t *wuss)
{
  result_t rc;
  int      i;

  if (wuss->ndirty == 0)
    return result_OK;

  rc = result_OK;
  for (i = 0; i < wuss->ndirty; i++)
  {
    wuss->scr->clip = wuss->dirty[i];
    wuss__fill_backdrop(wuss->scr, wuss->palette, &wuss->backdrop,
                        &wuss->dirty[i], 0, 0);

    redraw_from(wuss, wuss->z_order.next, &wuss->dirty[i], &rc);
  }

  box_reset(&wuss->scr->clip); /* see wuss_redraw's comment on the same call */

  wuss->ndirty = 0;

  return rc;
}
