/* redraw.c -- wuss - minimal window manager */

#include <string.h>

#include "geom/point.h"

#include "impl.h"

static void redraw_window(wuss_t *wuss, wuss_window_t *win, const box_t *full, result_t *rc)
{
  box_t clipped;
  box_t titlebar;
  box_t content;

  if (box_intersection(&win->visible, full, &clipped))
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

      pos.x = titlebar.x0 + 2;
      pos.y = titlebar.y0 + 2;
      bmfont_draw(wuss->font, wuss->scr, win->title, (int) strlen(win->title),
                 wuss->palette[wuss->titlebar_fg], wuss->palette[wuss->titlebar_bg],
                 &pos, NULL);
    }
  }

  wuss__content_box(win, &content);
  if (!box_intersection(&content, full, &clipped))
  {
    wuss->scr->clip = clipped;
    if (win->client.redraw != NULL)
    {
      result_t crc;

      crc = win->client.redraw(win, wuss->scr, &content, win->client.client_data);
      if (crc != result_OK)
        *rc = crc;
    }
  }

  if (!box_intersection(&win->visible, full, &clipped))
  {
    int      width, height;
    colour_t border;

    width  = win->visible.x1 - win->visible.x0;
    height = win->visible.y1 - win->visible.y0;
    border = wuss->palette[wuss->titlebar_bg];

    wuss->scr->clip = clipped;
    screen_draw_rect(wuss->scr, win->visible.x0,     win->visible.y0,     width, 1,      border);
    screen_draw_rect(wuss->scr, win->visible.x0,     win->visible.y1 - 1, width, 1,      border);
    screen_draw_rect(wuss->scr, win->visible.x0,     win->visible.y0,     1,     height, border);
    screen_draw_rect(wuss->scr, win->visible.x1 - 1, win->visible.y0,     1,     height, border);
  }
}

static void redraw_from(wuss_t *wuss, list_t *e, const box_t *full, result_t *rc)
{
  if (e == NULL)
    return;

  redraw_from(wuss, e->next, full, rc);
  redraw_window(wuss, (wuss_window_t *) e, full, rc);
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

  box_reset(&wuss->dirty);

  return rc;
}

result_t wuss_redraw_dirty(wuss_t *wuss)
{
  result_t rc;

  if (box_is_empty(&wuss->dirty))
    return result_OK;

  rc = result_OK;
  redraw_from(wuss, wuss->z_order.next, &wuss->dirty, &rc);

  box_reset(&wuss->dirty);

  return rc;
}
