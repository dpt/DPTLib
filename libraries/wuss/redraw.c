/* redraw.c -- wuss - minimal window manager */

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

  if (box_intersection(&win->visible, full, &visible_clipped))
    return; /* offscreen */

  wuss__furniture_draw(wuss, win, full);

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

    if (win->bg != wuss_NO_BACKGROUND)
      screen_draw_rect(wuss->scr,
                       content.x0, content.y0, box_size(&content),
                       wuss->palette[win->bg]);

    if (win->task.handle != NULL)
    {
      wuss_event_t event;
      result_t     crc;

      event.kind                 = wuss_EVENT_REDRAW;
      event.data.redraw.scr      = wuss->scr;
      event.data.redraw.content  = &pieces[i];
      event.data.redraw.bounds   = &content;
      event.data.redraw.scroll = win->scroll;
      crc = win->task.handle(win, &event, win->task.task_data);
      if (crc != result_OK)
        *rc = crc;
    }
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

    redraw_window(wuss, (wuss_window_t *) last, full, rc);
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

  if (wuss->backdrop != wuss_NO_BACKGROUND)
  {
    wuss->scr->clip = full;
    screen_draw_rect(wuss->scr, full.x0, full.y0, box_size(&full),
                     wuss->palette[wuss->backdrop]);
  }

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
    if (wuss->backdrop != wuss_NO_BACKGROUND)
    {
      wuss->scr->clip = wuss->dirty[i];
      screen_draw_rect(wuss->scr,
                       wuss->dirty[i].x0, wuss->dirty[i].y0, SIZE2D(wuss->dirty[i].x1 - wuss->dirty[i].x0, wuss->dirty[i].y1 - wuss->dirty[i].y0),
                       wuss->palette[wuss->backdrop]);
    }

    redraw_from(wuss, wuss->z_order.next, &wuss->dirty[i], &rc);
  }

  box_reset(&wuss->scr->clip); /* see wuss_redraw's comment on the same call */

  wuss->ndirty = 0;

  return rc;
}
