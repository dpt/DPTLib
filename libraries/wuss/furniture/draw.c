/* draw.c -- wuss - minimal window manager */

#include <string.h>

#include "geom/point.h"

#include "../impl.h"

void wuss__furniture_draw(wuss_t        *wuss,
                          wuss_window_t *window,
                          const box_t   *full)
{
  box_t visible_clipped;
  box_t clipped;
  box_t titlebar;

  if (box_intersection(&window->visible, full, &visible_clipped))
    return; /* offscreen */

  wuss__titlebar_box(window, &titlebar);
  if (!box_intersection(&titlebar, full, &clipped))
  {
    wuss->scr->clip = clipped;
    screen_draw_rect(wuss->scr,
                     titlebar.x0, titlebar.y0,
                     titlebar.x1 - titlebar.x0, titlebar.y1 - titlebar.y0,
                     wuss->palette[wuss->titlebar_bg]);

    if (wuss->font != NULL && window->title[0] != '\0')
    {
      point_t pos;
      int     text_x0;

      text_x0 = titlebar.x0 + 2;
      if (!(window->flags & wuss_WINDOW_NO_CLOSE))
      {
        box_t close;

        wuss__close_box(window, &close);
        text_x0 = close.x1 + 2;
      }

      pos.x = text_x0;
      pos.y = titlebar.y0 + 2;
      bmfont_draw(wuss->font, wuss->scr, window->title, (int) strlen(window->title),
                 wuss->palette[wuss->titlebar_fg], wuss->palette[wuss->titlebar_bg],
                 &pos, NULL);
    }

    if (!(window->flags & wuss_WINDOW_NO_CLOSE))
    {
      box_t close;

      wuss__close_box(window, &close);
      screen_draw_rect(wuss->scr,
                       close.x0, close.y0,
                       close.x1 - close.x0, close.y1 - close.y0,
                       wuss->palette[wuss->titlebar_fg]);
    }
  }

  if (!(window->flags & wuss_WINDOW_NO_OUTLINE))
  {
    int      width, height;
    colour_t border;

    width  = window->visible.x1 - window->visible.x0;
    height = window->visible.y1 - window->visible.y0;
    border = wuss->palette[wuss->titlebar_bg];

    wuss->scr->clip = visible_clipped;
    screen_draw_rect(wuss->scr, window->visible.x0,     window->visible.y0,     width, 1,      border);
    screen_draw_rect(wuss->scr, window->visible.x0,     window->visible.y1 - 1, width, 1,      border);
    screen_draw_rect(wuss->scr, window->visible.x0,     window->visible.y0,     1,     height, border);
    screen_draw_rect(wuss->scr, window->visible.x1 - 1, window->visible.y0,     1,     height, border);
  }
}
