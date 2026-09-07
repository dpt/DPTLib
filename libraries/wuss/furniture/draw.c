/* wuss/furniture/draw.c -- wuss - minimal window manager */

#include <string.h>

#include "base/utils.h"
#include "geom/point.h"

#include "../core/impl.h"

/* Paint one furniture rectangle "b" in "colour", clipped to the part of it
 * that falls inside "full" (the redraw region). A no-op when "b" is wholly
 * outside "full". Pins scr->clip to the clipped rect -- the caller's next
 * draw is expected to set its own clip. */
static void fill_furniture_rect(wuss_t      *wuss,
                                const box_t *b,
                                const box_t *full,
                                colour_t     colour)
{
  box_t clipped;

  if (box_intersection(b, full, &clipped))
    return; /* wholly outside the redraw region */

  wuss->scr->clip = clipped;
  screen_fill_rect(wuss->scr, b->x0, b->y0, box_size(b), colour);
}

void wuss__furniture_draw(wuss_t        *wuss,
                          wuss_window_t *window,
                          const box_t   *full)
{
  box_t     visible_clipped;
  box_t     clipped;
  box_t     titlebar;
  bmfont_t *titlefont;

  /* window titles are drawn in the bold weight (font slot 1) when one was
   * supplied, falling back to the system font */
  titlefont = (wuss->nfonts > 1 && wuss->fonts[1] != NULL)
            ? wuss->fonts[1]
            : wuss->fonts[0];

  if (box_intersection(&window->visible, full, &visible_clipped))
    return; /* offscreen */

  wuss__titlebar_box(window, &titlebar);
  if (!box_intersection(&titlebar, full, &clipped))
  {
    wuss->scr->clip = clipped;
    screen_fill_rect(wuss->scr,
                     titlebar.x0, titlebar.y0, box_size(&titlebar),
                     wuss->palette[wuss->furniture_colours.title.bg]);

    if (titlefont != NULL && window->title[0] != '\0')
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
        bmfont_measure(titlefont, window->title, titlelen, text_x1 - text_x0, &split_point, &width);

        pos.x = (split_point < titlelen) ? text_x0 : text_x0 + MAX(0, ((text_x1 - text_x0) - width) / 2);
        pos.y = titlebar.y0 + 2;
        wuss->scr->clip = text_clip;
        bmfont_draw(titlefont, wuss->scr, window->title, titlelen,
                   wuss->palette[wuss->furniture_colours.title.fg], wuss->palette[wuss->furniture_colours.title.bg],
                   &pos, NULL);
        wuss->scr->clip = clipped;
      }
    }

    if (!(window->flags & wuss_WINDOW_NO_CLOSE))
    {
      box_t close;

      wuss__close_box(window, &close);
      fill_furniture_rect(wuss, &close, full,
                          wuss->palette[wuss->furniture_colours.close]);
    }

    if (!(window->flags & wuss_WINDOW_NO_BACK))
    {
      box_t back;

      wuss__back_box(window, &back);
      fill_furniture_rect(wuss, &back, full,
                          wuss->palette[wuss->furniture_colours.back]);
    }

    if (!(window->flags & wuss_WINDOW_NO_TOGGLE_SIZE))
    {
      box_t toggle;

      wuss__toggle_box(window, &toggle);
      fill_furniture_rect(wuss, &toggle, full,
                          wuss->palette[wuss->furniture_colours.toggle]);
    }
  }

  /* With both scrollbars off but a resize icon on, the content box is still
   * carved back button_size on its right and bottom (see
   * wuss__furniture_carve_for) and no scroll strip paints that margin. It is
   * chrome, so furniture owns it: fill it here. Nothing pre-fills it
   * (fill_backdrop_excluding_content cuts the whole visible box), so this is
   * the only paint of the region -- no redundant fill, no flicker. */
  if ((window->flags & wuss_WINDOW_NO_VSCROLL) &&
      (window->flags & wuss_WINDOW_NO_HSCROLL) &&
      !(window->flags & wuss_WINDOW_NO_RESIZE))
  {
    box_t   content, band;
    point_t carve;
    int     outline_px;

    outline_px = wuss__outline_px(window);
    wuss__content_box(window, &content);
    wuss__furniture_carve_for(window->flags, wuss__button_size(window), &carve);

    if (carve.x > 0)
    {
      band.x0 = content.x1;
      band.x1 = window->visible.x1 - outline_px;
      band.y0 = window->visible.y0 + outline_px
              + wuss__titlebar_height(window);
      band.y1 = window->visible.y1 - outline_px;
      fill_furniture_rect(wuss, &band, full,
                          wuss->palette[wuss->furniture_colours.title.bg]);
    }

    if (carve.y > 0)
    {
      band.x0 = window->visible.x0 + outline_px;
      band.x1 = content.x1;
      band.y0 = content.y1;
      band.y1 = window->visible.y1 - outline_px;
      fill_furniture_rect(wuss, &band, full,
                          wuss->palette[wuss->furniture_colours.title.bg]);
    }
  }

  if (!(window->flags & wuss_WINDOW_NO_RESIZE))
  {
    box_t resize, rule;
    int   top_seam, left_seam;

    wuss__resize_box(window, &resize);

    /* Dividing seams in the 1px gap the scroll strips leave (scroll_strip
     * pulls each end in by WUSS_DIVIDER_PX): a rule along the resize box's top
     * edge (vscroll strip above) and its left edge (hscroll strip to the
     * left), in the interior rules' colour. */
    top_seam  = (window->flags & wuss_WINDOW_NO_VSCROLL) ? 0 : WUSS_DIVIDER_PX;
    left_seam = (window->flags & wuss_WINDOW_NO_HSCROLL) ? 0 : WUSS_DIVIDER_PX;

    fill_furniture_rect(wuss, &resize, full,
                        wuss->palette[wuss->furniture_colours.resize]);

    if (top_seam > 0)
    {
      rule.x0 = resize.x0 - left_seam;
      rule.x1 = resize.x1;
      rule.y0 = resize.y0 - top_seam;
      rule.y1 = resize.y0;
      fill_furniture_rect(wuss, &rule, full,
                          wuss->palette[wuss->furniture_colours.title.bg]);
    }

    if (left_seam > 0)
    {
      rule.x0 = resize.x0 - left_seam;
      rule.x1 = resize.x0;
      rule.y0 = resize.y0 - top_seam;
      rule.y1 = resize.y1;
      fill_furniture_rect(wuss, &rule, full,
                          wuss->palette[wuss->furniture_colours.title.bg]);
    }
  }

  if (!(window->flags & wuss_WINDOW_NO_VSCROLL))
  {
    box_t up, down, well, sausage;

    wuss__vscroll_up_box(window, &up);
    fill_furniture_rect(wuss, &up, full,
                        wuss->palette[wuss->furniture_colours.scroll.arrows]);

    wuss__vscroll_down_box(window, &down);
    fill_furniture_rect(wuss, &down, full,
                        wuss->palette[wuss->furniture_colours.scroll.arrows]);

    wuss__vscroll_well_box(window, &well);
    fill_furniture_rect(wuss, &well, full,
                        wuss->palette[wuss->furniture_colours.scroll.wells]);

    wuss__vscroll_sausage_box(window, &sausage);
    fill_furniture_rect(wuss, &sausage, full,
                        wuss->palette[wuss->furniture_colours.scroll.sausages]);
  }

  if (!(window->flags & wuss_WINDOW_NO_HSCROLL))
  {
    box_t left, right, well, sausage;

    wuss__hscroll_left_box(window, &left);
    fill_furniture_rect(wuss, &left, full,
                        wuss->palette[wuss->furniture_colours.scroll.arrows]);

    wuss__hscroll_right_box(window, &right);
    fill_furniture_rect(wuss, &right, full,
                        wuss->palette[wuss->furniture_colours.scroll.arrows]);

    wuss__hscroll_well_box(window, &well);
    fill_furniture_rect(wuss, &well, full,
                        wuss->palette[wuss->furniture_colours.scroll.wells]);

    wuss__hscroll_sausage_box(window, &sausage);
    fill_furniture_rect(wuss, &sausage, full,
                        wuss->palette[wuss->furniture_colours.scroll.sausages]);
  }

  {
    box_t   content, rule;
    point_t carve;

    /* Interior rules: where furniture is carved off the content area's right
     * or bottom edge, the last pixel of the carve is a dividing line. */
    wuss__content_box(window, &content);
    wuss__furniture_carve_for(window->flags, wuss__button_size(window), &carve);

    if (carve.x > 0)
    {
      rule.x0 = content.x1;
      rule.x1 = content.x1 + WUSS_DIVIDER_PX;
      rule.y0 = content.y0;
      rule.y1 = content.y1;
      fill_furniture_rect(wuss, &rule, full,
                          wuss->palette[wuss->furniture_colours.title.bg]);
    }

    if (carve.y > 0)
    {
      rule.x0 = content.x0;
      rule.x1 = content.x1 + ((carve.x > 0) ? WUSS_DIVIDER_PX : 0); /* meet the vertical rule at the corner */
      rule.y0 = content.y1;
      rule.y1 = content.y1 + WUSS_DIVIDER_PX;
      fill_furniture_rect(wuss, &rule, full,
                          wuss->palette[wuss->furniture_colours.title.bg]);
    }
  }

  if (!(window->flags & wuss_WINDOW_NO_OUTLINE))
  {
    int      width, height;
    colour_t border;

    width  = window->visible.x1 - window->visible.x0;
    height = window->visible.y1 - window->visible.y0;
    border = wuss->palette[wuss->furniture_colours.outline];

    wuss->scr->clip = visible_clipped;
    screen_fill_rect(wuss->scr, window->visible.x0,     window->visible.y0,     SIZE2D(width, 1), border);
    screen_fill_rect(wuss->scr, window->visible.x0,     window->visible.y1 - 1, SIZE2D(width, 1), border);
    screen_fill_rect(wuss->scr, window->visible.x0,     window->visible.y0,     SIZE2D(1, height), border);
    screen_fill_rect(wuss->scr, window->visible.x1 - 1, window->visible.y0,     SIZE2D(1, height), border);
  }
}
