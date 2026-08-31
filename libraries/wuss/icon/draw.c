/* draw.c -- wuss - draw a work-area icon */

#include <string.h>

#include "base/utils.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"
#include "framebuf/bmfont.h"
#include "framebuf/screen.h"

#include "../impl.h"

/* ----------------------------------------------------------------------- */

static void icon_bevel(screen_t    *scr,
                       const box_t *b,
                       colour_t     fill,
                       colour_t     light,
                       colour_t     dark)
{
  screen_draw_rect(scr, b->x0, b->y0,
                   SIZE2D(b->x1 - b->x0, b->y1 - b->y0), fill);

  screen_draw_line(scr, b->x0,     b->y0,     b->x1 - 1, b->y0,     light);
  screen_draw_line(scr, b->x0,     b->y0,     b->x0,     b->y1 - 1, light);
  screen_draw_line(scr, b->x0,     b->y1 - 1, b->x1 - 1, b->y1 - 1, dark);
  screen_draw_line(scr, b->x1 - 1, b->y0,     b->x1 - 1, b->y1 - 1, dark);
}

/* ----------------------------------------------------------------------- */

void wuss__icon_draw(wuss_t            *wuss,
                     const wuss_icon_t *icon,
                     const box_t       *content,
                     point_t            scroll)
{
  screen_t *scr;
  box_t     b;
  colour_t  fg;
  int       font_width, font_height;
  int       have_font;

  if (icon->flags & wuss_ICON_FLAGS_HIDDEN)
    return;

  scr = wuss->scr;

  wuss__icon_box_to_screen(content, scroll, &icon->bbox, &b);

  if (b.x1 <= b.x0 || b.y1 <= b.y0)
    return;

  fg = wuss->palette[icon->fg];

  have_font = (wuss->font != NULL && icon->text[0] != '\0');
  if (have_font)
    bmfont_get_info(wuss->font, &font_width, &font_height);
  else
    font_width = font_height = 0;
  NOT_USED(font_width);

  switch (icon->type)
  {
  case wuss_ICON_TYPE_PATTERN:
    {
      colour_t pat_fg;

      /* disabled: fold the pattern into its own ground so it reads as greyed,
       * mirroring the button's fg-swap */
      pat_fg = (icon->flags & wuss_ICON_FLAGS_DISABLED)
             ? wuss->palette[icon->bg]
             : fg;

      screen_fill_pattern(scr, &b, icon->pattern,
                          content->x0 - scroll.x, content->y0 - scroll.y,
                          pat_fg, wuss->palette[icon->bg]);
    }
    break;

  case wuss_ICON_TYPE_LABEL:
    {
      colour_t bg;

      if (icon->bg != wuss_NO_BACKGROUND)
      {
        bg = wuss->palette[icon->bg];
        screen_draw_rect(scr, b.x0, b.y0,
                         SIZE2D(b.x1 - b.x0, b.y1 - b.y0), bg);
      }
      else if (icon->window->bg.colour != wuss_NO_BACKGROUND)
      {
        /* blend against the dominant backdrop colour: for a pattern fill
         * that's its background (clear-bit) colour, not the foreground */
        bg = (icon->window->bg.pattern != screen_PATTERN_SOLID)
           ? wuss->palette[icon->window->bg.pattern_bg]
           : wuss->palette[icon->window->bg.colour];
      }
      else
      {
        bg = fg; /* bmfont needs a blend colour; nothing better to offer */
      }

      if (have_font)
      {
        point_t        pos;
        int            interior_w, split_point;
        bmfont_width_t width;

        interior_w = (b.x1 - b.x0) - 2;
        if (interior_w < 1)
          interior_w = 1;

        bmfont_measure(wuss->font, icon->text, (int) strlen(icon->text),
                       interior_w, &split_point, &width);

        if (icon->flags & wuss_ICON_FLAGS_JUSTIFY_CENTRE)
          pos.x = b.x0 + ((b.x1 - b.x0) - width) / 2;
        else if (icon->flags & wuss_ICON_FLAGS_JUSTIFY_RIGHT)
          pos.x = b.x1 - 1 - width;
        else
          pos.x = b.x0 + 1;
        pos.y = b.y0 + (b.y1 - b.y0 - font_height) / 2;

        bmfont_draw(wuss->font, scr, icon->text, (int) strlen(icon->text),
                    fg, bg, &pos, NULL);
      }
    }
    break;

  case wuss_ICON_TYPE_FRAME:
    {
      colour_t bg;
      int      cap_w, cap_x, gap_x0, gap_x1, mid_y;

      /* the caption sits over whatever ground the label case would blend
       * against: an explicit bg, else the window's dominant backdrop, else
       * fall back to fg so bmfont still has a blend colour */
      if (icon->bg != wuss_NO_BACKGROUND)
        bg = wuss->palette[icon->bg];
      else if (icon->window->bg.colour != wuss_NO_BACKGROUND)
        bg = (icon->window->bg.pattern != screen_PATTERN_SOLID)
           ? wuss->palette[icon->window->bg.pattern_bg]
           : wuss->palette[icon->window->bg.colour];
      else
        bg = fg;

      cap_w = 0;
      if (have_font)
      {
        int            split_point;
        bmfont_width_t width;

        bmfont_measure(wuss->font, icon->text, (int) strlen(icon->text),
                       (b.x1 - b.x0) - WUSS_FRAME_CAPTION_INSET * 2,
                       &split_point, &width);
        cap_w = width;
      }

      mid_y = b.y0 + font_height / 2;

      /* left, right and bottom edges are unbroken */
      screen_draw_line(scr, b.x0, mid_y, b.x0, b.y1 - 1, fg);
      screen_draw_line(scr, b.x1 - 1, mid_y, b.x1 - 1, b.y1 - 1, fg);
      screen_draw_line(scr, b.x0, b.y1 - 1, b.x1 - 1, b.y1 - 1, fg);

      /* the top edge is broken around the caption */
      cap_x  = b.x0 + WUSS_FRAME_CAPTION_INSET;
      gap_x0 = cap_x - WUSS_FRAME_CAPTION_PAD;
      gap_x1 = cap_x + cap_w + WUSS_FRAME_CAPTION_PAD;
      if (gap_x0 > b.x0)
        screen_draw_line(scr, b.x0, mid_y, gap_x0, mid_y, fg);
      if (gap_x1 < b.x1 - 1)
        screen_draw_line(scr, gap_x1, mid_y, b.x1 - 1, mid_y, fg);

      if (have_font && cap_w > 0)
      {
        point_t pos;

        pos.x = cap_x;
        pos.y = b.y0;
        bmfont_draw(wuss->font, scr, icon->text, (int) strlen(icon->text),
                    fg, bg, &pos, NULL);
      }
    }
    break;

  case wuss_ICON_TYPE_BUTTON:
    {
      colour_t light, dark, base, label;
      int      pressed, is_default;

      pressed    = icon->pressed;
      is_default = (icon->flags & wuss_ICON_FLAGS_DEFAULT) != 0;

      if (is_default)
      {
        /* default action button: a flat accent-filled rectangle inside a
         * one-pixel accent-text border, distinct from the bevelled ordinary
         * buttons around it */
        base  = wuss->palette[wuss->accent_bg];
        light = wuss->palette[wuss->accent_fg];
        dark  = light;
        label = wuss->palette[wuss->accent_fg];
      }
      else
      {
        base  = wuss->palette[icon->bg];
        light = wuss->palette[wuss->bevel_light];
        dark  = wuss->palette[wuss->bevel_dark];
        label = fg;
      }

      if (icon->flags & wuss_ICON_FLAGS_DISABLED)
        label = wuss->palette[wuss->bevel_dark]; /* greyed: sink toward dark */

      if (pressed)
        icon_bevel(scr, &b, base, dark, light);
      else
        icon_bevel(scr, &b, base, light, dark);

      if (have_font)
      {
        point_t        pos;
        int            interior_w, split_point;
        bmfont_width_t width;

        interior_w = (b.x1 - b.x0) - 2;
        if (interior_w < 1)
          interior_w = 1;

        bmfont_measure(wuss->font, icon->text, (int) strlen(icon->text),
                       interior_w, &split_point, &width);

        pos.x = b.x0 + ((b.x1 - b.x0) - width) / 2;
        pos.y = b.y0 + (b.y1 - b.y0 - font_height) / 2;
        if (pressed)
        {
          pos.x += 1;
          pos.y += 1;
        }

        bmfont_draw(wuss->font, scr, icon->text, (int) strlen(icon->text),
                    label, base, &pos, NULL);
      }
    }
    break;
  }
}
