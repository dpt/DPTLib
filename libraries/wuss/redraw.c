/* redraw.c -- wuss - minimal window manager */

#include <string.h>

#include "geom/point.h"

#include "impl.h"

static void redraw_window(wuss_t        *wuss,
                          wuss_window_t *win,
                          const box_t   *full,
                          result_t      *rc)
{
  box_t clipped;
  box_t visible_clipped;
  box_t titlebar;
  box_t content;

  if (box_intersection(&win->visible, full, &visible_clipped))
    return; /* offscreen */

  wuss__titlebar_box(win, &titlebar);
  if (!box_intersection(&titlebar, full, &clipped))
  {
    wuss->scr->clip = clipped;
    screen_draw_rect(wuss->scr,
                     titlebar.x0, titlebar.y0,
                     titlebar.x1 - titlebar.x0, titlebar.y1 - titlebar.y0,
                     wuss->palette[wuss->titlebar_bg]);

    if (wuss->font != NULL && win->title[0] != '\0')
    {
      point_t pos;
      int     text_x0;

      text_x0 = titlebar.x0 + 2;
      if (!(win->flags & wuss_WINDOW_NO_CLOSE))
      {
        box_t close;

        wuss__close_box(win, &close);
        text_x0 = close.x1 + 2;
      }

      pos.x = text_x0;
      pos.y = titlebar.y0 + 2;
      bmfont_draw(wuss->font, wuss->scr, win->title, (int) strlen(win->title),
                 wuss->palette[wuss->titlebar_fg], wuss->palette[wuss->titlebar_bg],
                 &pos, NULL);
    }

    if (!(win->flags & wuss_WINDOW_NO_CLOSE))
    {
      box_t close;

      wuss__close_box(win, &close);
      screen_draw_rect(wuss->scr,
                       close.x0, close.y0,
                       close.x1 - close.x0, close.y1 - close.y0,
                       wuss->palette[wuss->titlebar_fg]);
    }
  }

  wuss__content_box(win, &content);
  if (!box_intersection(&content, full, &clipped))
  {
    wuss->scr->clip = clipped;

    if (win->task.bg != wuss_NO_BACKGROUND)
      screen_draw_rect(wuss->scr,
                       content.x0, content.y0,
                       content.x1 - content.x0, content.y1 - content.y0,
                       wuss->palette[win->task.bg]);

    if (win->task.handle != NULL)
    {
      wuss_event_t event;
      result_t     crc;

      event.kind             = wuss_EVENT_REDRAW;
      event.data.redraw.scr     = wuss->scr;
      event.data.redraw.content = &content;
      crc = win->task.handle(win, &event, win->task.task_data);
      if (crc != result_OK)
        *rc = crc;
    }
  }

  if (!(win->flags & wuss_WINDOW_NO_OUTLINE))
  {
    int      width, height;
    colour_t border;

    width  = win->visible.x1 - win->visible.x0;
    height = win->visible.y1 - win->visible.y0;
    border = wuss->palette[wuss->titlebar_bg];

    wuss->scr->clip = visible_clipped;
    screen_draw_rect(wuss->scr, win->visible.x0,     win->visible.y0,     width, 1,      border);
    screen_draw_rect(wuss->scr, win->visible.x0,     win->visible.y1 - 1, width, 1,      border);
    screen_draw_rect(wuss->scr, win->visible.x0,     win->visible.y0,     1,     height, border);
    screen_draw_rect(wuss->scr, win->visible.x1 - 1, win->visible.y0,     1,     height, border);
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
  full.x1 = wuss->scr->width;
  full.y1 = wuss->scr->height;

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
    redraw_from(wuss, wuss->z_order.next, &wuss->dirty[i], &rc);

  box_reset(&wuss->scr->clip); /* see wuss_redraw's comment on the same call */

  wuss->ndirty = 0;

  return rc;
}
