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
  box_t                      slot;
  const wuss_iconbar_icon_t *icon;
  int                        h;

  wuss__iconbar_slot_box(wuss, index, &slot);

  icon = wuss->iconbar_icons[index];
  h    = (icon->spec.image != NULL) ? icon->spec.image->size.h
                                    : WUSS_ICONBAR_ICON;

  out->x0 = slot.x0;
  out->y0 = slot.y0 + (WUSS_ICONBAR_HEIGHT - h) / 2;
  out->x1 = out->x0 + WUSS_ICONBAR_ICON;
  out->y1 = out->y0 + h;
}

/* ----------------------------------------------------------------------- */

static void iconbar_draw_icon(wuss_t                    *wuss,
                              const wuss_iconbar_icon_t *icon,
                              const box_t               *icon_box)
{
  colour_t bg, ink;

  /* RISC OS style: no border and no pressed look -- the image and label sit
   * straight on the bar's backdrop */
  bg  = wuss->palette[wuss__resolve_colour(wuss, wuss_COLOUR_WINDOW)];
  ink = wuss->palette[wuss__resolve_colour(wuss, wuss_COLOUR_BLACK)];

  if (icon->spec.image != NULL)
  {
    screen_t clipped;
    int      x, y;

    clipped = *wuss->scr;
    if (box_intersection(&wuss->scr->clip, icon_box, &clipped.clip))
      return;

    x = icon_box->x0 +
        ((icon_box->x1 - icon_box->x0) - icon->spec.image->size.w) / 2;
    y = icon_box->y0 +
        ((icon_box->y1 - icon_box->y0) - icon->spec.image->size.h) / 2;
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
    wuss__text_draw(font, wuss->scr, icon->spec.text, len, ink, bg, &pos,
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
