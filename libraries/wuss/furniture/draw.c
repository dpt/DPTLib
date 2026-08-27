/* draw.c -- wuss - minimal window manager */

#include <string.h>

#include "base/utils.h"
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
                     wuss->palette[wuss->furniture_colours.title.bg]);

    if (wuss->font != NULL && window->title[0] != '\0')
    {
      point_t        pos;
      int            text_x0, text_x1, titlelen, split_point;
      bmfont_width_t width;
      box_t          text_box, text_clip;

      text_x0 = titlebar.x0 + 2;
      if (!(window->flags & wuss_WINDOW_NO_CLOSE))
      {
        box_t close;

        wuss__close_box(window, &close);
        text_x0 = close.x1 + 2;
      }
      else if (!(window->flags & wuss_WINDOW_NO_BACK))
      {
        box_t back;

        wuss__back_box(window, &back);
        text_x0 = back.x1 + 2;
      }

      text_x1 = titlebar.x1 - 2;
      if (!(window->flags & wuss_WINDOW_NO_TOGGLE_SIZE))
      {
        box_t toggle;

        wuss__toggle_box(window, &toggle);
        text_x1 = toggle.x0 - 2;
      }

      /* A title too wide for its slot mustn't bleed into a neighbouring
       * icon: clip drawing to the slot itself, not just the whole titlebar,
       * so a too-long title is cut off cleanly rather than overdrawing
       * whatever furniture the current redraw didn't happen to touch. */
      text_box.x0 = text_x0;
      text_box.y0 = titlebar.y0;
      text_box.x1 = text_x1;
      text_box.y1 = titlebar.y1;
      if (text_x1 > text_x0 && !box_intersection(&text_box, &clipped, &text_clip))
      {
        titlelen = (int) strlen(window->title);
        bmfont_measure(wuss->font, window->title, titlelen, text_x1 - text_x0, &split_point, &width);

        pos.x = (split_point < titlelen) ? text_x0 : text_x0 + MAX(0, ((text_x1 - text_x0) - width) / 2);
        pos.y = titlebar.y0 + 2;
        wuss->scr->clip = text_clip;
        bmfont_draw(wuss->font, wuss->scr, window->title, titlelen,
                   wuss->palette[wuss->furniture_colours.title.fg], wuss->palette[wuss->furniture_colours.title.bg],
                   &pos, NULL);
        wuss->scr->clip = clipped;
      }
    }

    if (!(window->flags & wuss_WINDOW_NO_CLOSE))
    {
      box_t close;

      wuss__close_box(window, &close);
      screen_draw_rect(wuss->scr,
                       close.x0, close.y0,
                       close.x1 - close.x0, close.y1 - close.y0,
                       wuss->palette[wuss->furniture_colours.close]);
    }

    if (!(window->flags & wuss_WINDOW_NO_BACK))
    {
      box_t back;

      wuss__back_box(window, &back);
      screen_draw_rect(wuss->scr,
                       back.x0, back.y0,
                       back.x1 - back.x0, back.y1 - back.y0,
                       wuss->palette[wuss->furniture_colours.back]);
    }

    if (!(window->flags & wuss_WINDOW_NO_TOGGLE_SIZE))
    {
      box_t toggle;

      wuss__toggle_box(window, &toggle);
      screen_draw_rect(wuss->scr,
                       toggle.x0, toggle.y0,
                       toggle.x1 - toggle.x0, toggle.y1 - toggle.y0,
                       wuss->palette[wuss->furniture_colours.toggle]);
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
                       wuss->palette[wuss->furniture_colours.resize]);
    }
  }

  if (!(window->flags & wuss_WINDOW_NO_VSCROLL))
  {
    box_t up, down, well, sausage;

    wuss__vscroll_up_box(window, &up);
    if (!box_intersection(&up, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr, up.x0, up.y0, up.x1 - up.x0, up.y1 - up.y0,
                       wuss->palette[wuss->furniture_colours.arrows]);
    }

    wuss__vscroll_down_box(window, &down);
    if (!box_intersection(&down, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr, down.x0, down.y0, down.x1 - down.x0, down.y1 - down.y0,
                       wuss->palette[wuss->furniture_colours.arrows]);
    }

    wuss__vscroll_well_box(window, &well);
    if (!box_intersection(&well, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr, well.x0, well.y0, well.x1 - well.x0, well.y1 - well.y0,
                       wuss->palette[wuss->furniture_colours.wells]);
    }

    wuss__vscroll_sausage_box(window, &sausage);
    if (!box_intersection(&sausage, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr, sausage.x0, sausage.y0, sausage.x1 - sausage.x0, sausage.y1 - sausage.y0,
                       wuss->palette[wuss->furniture_colours.sausages]);
    }
  }

  if (!(window->flags & wuss_WINDOW_NO_HSCROLL))
  {
    box_t left, right, well, sausage;

    wuss__hscroll_left_box(window, &left);
    if (!box_intersection(&left, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr, left.x0, left.y0, left.x1 - left.x0, left.y1 - left.y0,
                       wuss->palette[wuss->furniture_colours.arrows]);
    }

    wuss__hscroll_right_box(window, &right);
    if (!box_intersection(&right, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr, right.x0, right.y0, right.x1 - right.x0, right.y1 - right.y0,
                       wuss->palette[wuss->furniture_colours.arrows]);
    }

    wuss__hscroll_well_box(window, &well);
    if (!box_intersection(&well, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr, well.x0, well.y0, well.x1 - well.x0, well.y1 - well.y0,
                       wuss->palette[wuss->furniture_colours.wells]);
    }

    wuss__hscroll_sausage_box(window, &sausage);
    if (!box_intersection(&sausage, full, &clipped))
    {
      wuss->scr->clip = clipped;
      screen_draw_rect(wuss->scr, sausage.x0, sausage.y0, sausage.x1 - sausage.x0, sausage.y1 - sausage.y0,
                       wuss->palette[wuss->furniture_colours.sausages]);
    }
  }

  if (!(window->flags & wuss_WINDOW_NO_OUTLINE))
  {
    int      width, height;
    colour_t border;

    width  = window->visible.x1 - window->visible.x0;
    height = window->visible.y1 - window->visible.y0;
    border = wuss->palette[wuss->furniture_colours.title.bg]; /* no dedicated outline class; matches titlebar chrome */

    wuss->scr->clip = visible_clipped;
    screen_draw_rect(wuss->scr, window->visible.x0,     window->visible.y0,     width, 1,      border);
    screen_draw_rect(wuss->scr, window->visible.x0,     window->visible.y1 - 1, width, 1,      border);
    screen_draw_rect(wuss->scr, window->visible.x0,     window->visible.y0,     1,     height, border);
    screen_draw_rect(wuss->scr, window->visible.x1 - 1, window->visible.y0,     1,     height, border);
  }
}
