/* wuss/iconbar/draw.c -- icon bar geometry and painting */

#include <limits.h>
#include <string.h>

#include "geom/box.h"
#include "framebuf/screen.h"

#include "../core/impl.h"
#include "../font/font.h"

#ifdef WUSS_ICONBAR

void wuss__iconbar_box(const wuss_t *wuss, box_t *out)
{
  out->x0 = 0;
  out->y0 = wuss->scr->size.h - WUSS_ICONBAR_HEIGHT;
  out->x1 = wuss->scr->size.w;
  out->y1 = wuss->scr->size.h;
}

void wuss__iconbar_slot_box(const wuss_t *wuss, int index, box_t *out)
{
  box_t bar;

  wuss__iconbar_box(wuss, &bar);

  out->x0 = bar.x0 + index * WUSS_ICONBAR_SLOT;
  out->y0 = bar.y0;
  out->x1 = out->x0 + WUSS_ICONBAR_SLOT;
  out->y1 = bar.y1;
}

/* ----------------------------------------------------------------------- */

static void iconbar_draw_icon(wuss_t                    *wuss,
                              const wuss_iconbar_icon_t *icon,
                              const box_t               *slot)
{
  colour_t face, light, dark, ink;
  int      pressed;

  pressed = wuss__iconbar_icon_pressed(icon);

  face  = wuss->palette[wuss__resolve_colour(wuss, wuss_COLOUR_GREY)];
  light = wuss->palette[wuss__resolve_colour(wuss, wuss_COLOUR_WHITE)];
  dark  = wuss->palette[wuss__resolve_colour(wuss, wuss_COLOUR_BLACK)];
  ink   = dark;

  screen_fill_rect(wuss->scr, slot->x0, slot->y0, box_size(slot), face);
  screen_draw_bevel_edge(wuss->scr, slot, pressed ? dark : light,
                                         pressed ? light : dark);

  if (icon->spec.image != NULL)
  {
    box_t    inner;
    screen_t clipped;
    int      x, y;

    inner = box_grown(slot, -WUSS_ICONBAR_BEVEL);
    clipped = *wuss->scr;
    if (box_intersection(&wuss->scr->clip, &inner, &clipped.clip))
      return;

    x = inner.x0 + ((inner.x1 - inner.x0) - icon->spec.image->size.w) / 2;
    y = inner.y0 + ((inner.y1 - inner.y0) - icon->spec.image->size.h) / 2;
    screen_copy_bitmap(&clipped, x, y, icon->spec.image);
  }

  if (icon->spec.text[0] != '\0' && wuss->fonts.fonts[0] != NULL)
  {
    bmfont_t      *font;
    bmfont_width_t width;
    int            len, ascent;
    point_t        pos;

    font = wuss->fonts.fonts[0];
    len  = (int) strlen(icon->spec.text);
    wuss__text_measure(font, icon->spec.text, len, INT_MAX, NULL, &width);
    bmfont_get_info(font, NULL, NULL, &ascent, NULL);

    pos.x = slot->x0 + ((slot->x1 - slot->x0) - (int) width) / 2;
    pos.y = slot->y1 - WUSS_ICONBAR_BEVEL - WUSS_ICONBAR_TEXT_PAD;
    wuss__text_draw(font, wuss->scr, icon->spec.text, len, ink, face, &pos,
                    NULL);
  }
}

void wuss__iconbar_draw(wuss_t *wuss, const box_t *clip)
{
  box_t bar, saved_clip;
  int   i;

  wuss__iconbar_box(wuss, &bar);
  if (box_intersection(clip, &bar, &bar))
    return;

  saved_clip     = wuss->scr->clip;
  if (box_intersection(&saved_clip, &bar, &wuss->scr->clip))
  {
    wuss->scr->clip = saved_clip;
    return;
  }

  screen_fill_rect(wuss->scr, bar.x0, bar.y0, box_size(&bar),
                   wuss->palette[wuss__resolve_colour(wuss, wuss_COLOUR_GREY)]);

  for (i = 0; i < wuss->niconbar_icons; i++)
  {
    box_t slot;

    wuss__iconbar_slot_box(wuss, i, &slot);
    if (slot.x0 >= wuss->scr->size.w)
      break;

    iconbar_draw_icon(wuss, wuss->iconbar_icons[i], &slot);
  }

  wuss->scr->clip = saved_clip;
}

#endif /* WUSS_ICONBAR */
