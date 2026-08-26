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

    if (!(window->flags & wuss_WINDOW_NO_BACK))
    {
      box_t back;

      wuss__back_box(window, &back);
      screen_draw_rect(wuss->scr,
                       back.x0, back.y0,
                       back.x1 - back.x0, back.y1 - back.y0,
                       wuss->palette[wuss->titlebar_fg]);
    }

    if (!(window->flags & wuss_WINDOW_NO_TOGGLE_SIZE))
    {
      box_t toggle;

      wuss__toggle_box(window, &toggle);
      screen_draw_rect(wuss->scr,
                       toggle.x0, toggle.y0,
                       toggle.x1 - toggle.x0, toggle.y1 - toggle.y0,
                       wuss->palette[wuss->titlebar_fg]);
    }
  }

  if (!(window->flags & wuss_WINDOW_NO_RESIZE))
  {
    box_t resize;

    wuss__resize_box(window, &resize);
    if (!box_intersection(&resize, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr,
                       resize.x0, resize.y0,
                       resize.x1 - resize.x0, resize.y1 - resize.y0,
                       wuss->palette[wuss->titlebar_bg]);
    }
  }

  if (!(window->flags & wuss_WINDOW_NO_VSCROLL))
  {
    box_t up, down, thumb;

    wuss__vscroll_up_box(window, &up);
    if (!box_intersection(&up, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr, up.x0, up.y0, up.x1 - up.x0, up.y1 - up.y0,
                       wuss->palette[wuss->titlebar_bg]);
    }

    wuss__vscroll_down_box(window, &down);
    if (!box_intersection(&down, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr, down.x0, down.y0, down.x1 - down.x0, down.y1 - down.y0,
                       wuss->palette[wuss->titlebar_bg]);
    }

    wuss__vscroll_thumb_box(window, &thumb);
    if (!box_intersection(&thumb, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr, thumb.x0, thumb.y0, thumb.x1 - thumb.x0, thumb.y1 - thumb.y0,
                       wuss->palette[wuss->titlebar_fg]);
    }
  }

  if (!(window->flags & wuss_WINDOW_NO_HSCROLL))
  {
    box_t left, right, thumb;

    wuss__hscroll_left_box(window, &left);
    if (!box_intersection(&left, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr, left.x0, left.y0, left.x1 - left.x0, left.y1 - left.y0,
                       wuss->palette[wuss->titlebar_bg]);
    }

    wuss__hscroll_right_box(window, &right);
    if (!box_intersection(&right, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr, right.x0, right.y0, right.x1 - right.x0, right.y1 - right.y0,
                       wuss->palette[wuss->titlebar_bg]);
    }

    wuss__hscroll_thumb_box(window, &thumb);
    if (!box_intersection(&thumb, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr, thumb.x0, thumb.y0, thumb.x1 - thumb.x0, thumb.y1 - thumb.y0,
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
