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
  if (wuss->iconbar_window == NULL)
  {
    box_reset(out);
    return;
  }

  *out = wuss->iconbar_window->visible;
}

void wuss__iconbar_slot_box(const wuss_t *wuss, int index, box_t *out)
{
  box_t bar;

  wuss__iconbar_box(wuss, &bar);

  out->x0 = bar.x0 + WUSS_ICONBAR_GAP + index * WUSS_ICONBAR_SLOT;
  out->y0 = bar.y0;
  out->x1 = out->x0 + WUSS_ICONBAR_SLOT;
  out->y1 = bar.y1;
}

void wuss__iconbar_icon_box(const wuss_t *wuss, int index, box_t *out)
{
  box_t slot;

  wuss__iconbar_slot_box(wuss, index, &slot);

  out->x0 = slot.x0;
  out->y0 = slot.y0 + WUSS_ICONBAR_OUTLINE + WUSS_ICONBAR_TOP_SPARE;
  out->x1 = out->x0 + WUSS_ICONBAR_ICON;
  out->y1 = out->y0 + WUSS_ICONBAR_ICON;
}

/* ----------------------------------------------------------------------- */

static void iconbar_draw_icon(wuss_t                    *wuss,
                              const wuss_iconbar_icon_t *icon,
                              const box_t               *icon_box)
{
  colour_t face, light, dark, ink;
  int      pressed;

  pressed = wuss__iconbar_icon_pressed(icon);

  face  = wuss->palette[wuss__resolve_colour(wuss, wuss_COLOUR_GREY)];
  light = wuss->palette[wuss__resolve_colour(wuss, wuss_COLOUR_WHITE)];
  dark  = wuss->palette[wuss__resolve_colour(wuss, wuss_COLOUR_BLACK)];
  ink   = dark;

  screen_fill_rect(wuss->scr, icon_box->x0, icon_box->y0,
                              box_size(icon_box), face);
  screen_draw_bevel_edge(wuss->scr, icon_box, pressed ? dark : light,
                                             pressed ? light : dark);

  if (icon->spec.image != NULL)
  {
    box_t    inner;
    screen_t clipped;
    int      x, y;

    inner = box_grown(icon_box, -WUSS_ICONBAR_BEVEL);
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

    pos.x = icon_box->x0 + ((icon_box->x1 - icon_box->x0) - (int) width) / 2;
    pos.y = icon_box->y1 + WUSS_ICONBAR_TEXT_PAD + ascent;
    wuss__text_draw(font, wuss->scr, icon->spec.text, len, ink, face, &pos,
                    NULL);
  }
}

void wuss__iconbar_draw_icons(wuss_t *wuss, const box_t *piece)
{
  int i;

  for (i = 0; i < wuss->niconbar_icons; i++)
  {
    box_t slot, icon_box, clipped;

    wuss__iconbar_slot_box(wuss, i, &slot);
    if (slot.x0 >= wuss->scr->size.w)
      break;

    if (box_intersection(piece, &slot, &clipped))
      continue;

    wuss__iconbar_icon_box(wuss, i, &icon_box);
    iconbar_draw_icon(wuss, wuss->iconbar_icons[i], &icon_box);
  }
}

#endif /* WUSS_ICONBAR */
