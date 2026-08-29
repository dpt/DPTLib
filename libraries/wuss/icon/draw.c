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

  b.x0 = content->x0 - scroll.x + icon->bbox.x0;
  b.y0 = content->y0 - scroll.y + icon->bbox.y0;
  b.x1 = content->x0 - scroll.x + icon->bbox.x1;
  b.y1 = content->y0 - scroll.y + icon->bbox.y1;

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
  case wuss_ICON_TYPE_LABEL:
    {
      colour_t bg;

      if (icon->bg != wuss_NO_BACKGROUND)
      {
        bg = wuss->palette[icon->bg];
        screen_draw_rect(scr, b.x0, b.y0,
                         SIZE2D(b.x1 - b.x0, b.y1 - b.y0), bg);
      }
      else if (icon->window->bg != wuss_NO_BACKGROUND)
      {
        bg = wuss->palette[icon->window->bg];
      }
      else
      {
        bg = fg; /* bmfont needs a blend colour; nothing better to offer */
      }

      if (have_font)
      {
        point_t pos;

        pos.x = b.x0 + 1;
        pos.y = b.y0 + (b.y1 - b.y0 - font_height) / 2;
        bmfont_draw(wuss->font, scr, icon->text, (int) strlen(icon->text),
                    fg, bg, &pos, NULL);
      }
    }
    break;

  case wuss_ICON_TYPE_BUTTON:
    {
      colour_t light, dark, base;
      int      pressed;

      base    = wuss->palette[icon->bg];
      light   = wuss->palette[wuss->bevel_light];
      dark    = wuss->palette[wuss->bevel_dark];
      pressed = icon->pressed;

      if (icon->flags & wuss_ICON_FLAGS_DISABLED)
        fg = dark; /* greyed: label sinks toward the dark bevel shade */

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
                    fg, base, &pos, NULL);
      }
    }
    break;
  }
}
