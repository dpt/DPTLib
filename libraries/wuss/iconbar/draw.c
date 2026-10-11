/* wuss/iconbar/draw.c -- icon bar geometry and painting */

#include <limits.h>
#include <string.h>

#include "base/utils.h"
#include "geom/box.h"
#include "framebuf/screen.h"

#include "../core/impl.h"
#include "../font/font.h"

#ifdef WUSS_ICONBAR

void wuss__iconbar_box(const wuss_t *wuss, box_t *out)
{
  if (wuss->iconbar.window == NULL)
  {
    box_reset(out);
    return;
  }

  *out = wuss->iconbar.window->visible;
}

/* Width of icon "icon": the label's width if it has one (never narrower than
 * the image), else the image's width, else WUSS_ICONBAR_ICON. */
static int iconbar_icon_width(const wuss_t              *wuss,
                              const wuss_iconbar_icon_t *icon)
{
  bmfont_width_t width;
  int            w;

  w = (icon->spec.image != NULL) ? icon->spec.image->size.w : 0;

  if (icon->spec.text[0] != '\0' && wuss->fonts.fonts[0] != NULL)
  {
    wuss__text_measure(wuss->fonts.fonts[0], icon->spec.text,
                       (int) strlen(icon->spec.text), INT_MAX, NULL, &width);
    w = MAX(w, (int) width);
  }

  return (w > 0) ? w : WUSS_ICONBAR_ICON;
}

void wuss__iconbar_slot_box(const wuss_t *wuss, int index, box_t *out)
{
  box_t                      bar;
  const wuss_iconbar_icon_t *icon;
  int                        i, w, edge;

  wuss__iconbar_box(wuss, &bar);

  icon = wuss->iconbar.icons[index];
  w    = iconbar_icon_width(wuss, icon);

  /* icons of one side pack from that side's edge in creation order, with
   * WUSS_ICONBAR_GAP of bar between neighbours and at the edge */
  if (icon->spec.side == wuss_ICONBAR_RIGHT)
  {
    edge = bar.x1 - WUSS_ICONBAR_GAP;
    for (i = 0; i < index; i++)
    {
      if (wuss->iconbar.icons[i]->spec.side == wuss_ICONBAR_RIGHT)
        edge -= iconbar_icon_width(wuss, wuss->iconbar.icons[i]) +
                WUSS_ICONBAR_GAP;
    }
    out->x1 = edge;
    out->x0 = edge - w;
  }
  else
  {
    edge = bar.x0 + WUSS_ICONBAR_GAP;
    for (i = 0; i < index; i++)
    {
      if (wuss->iconbar.icons[i]->spec.side != wuss_ICONBAR_RIGHT)
        edge += iconbar_icon_width(wuss, wuss->iconbar.icons[i]) +
                WUSS_ICONBAR_GAP;
    }
    out->x0 = edge;
    out->x1 = edge + w;
  }

  out->y0 = bar.y0;
  out->y1 = bar.y1;
}

void wuss__iconbar_icon_box(const wuss_t *wuss, int index, box_t *out)
{
  box_t                      slot;
  const wuss_iconbar_icon_t *icon;
  int                        h, block, font_h;

  wuss__iconbar_slot_box(wuss, index, &slot);

  icon  = wuss->iconbar.icons[index];
  h     = (icon->spec.image != NULL) ? icon->spec.image->size.h
                                     : WUSS_ICONBAR_ICON;
  block = h;

  if (icon->spec.text[0] != '\0' && wuss->fonts.fonts[0] != NULL)
  {
    bmfont_get_info(wuss->fonts.fonts[0], NULL, &font_h, NULL, NULL);
    block += WUSS_ICONBAR_TEXT_PAD + font_h;
  }

  /* spare space splits 5:3 above:below, rounded to nearest */
  out->x0 = slot.x0;
  out->y0 = slot.y0 + ((WUSS_ICONBAR_HEIGHT - block) * 5 + 4) / 8;
  out->x1 = slot.x1;
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

    /* an empty intersection only skips the image: the label lies below
     * icon_box, so it may still be inside the clip */
    clipped = *wuss->scr;
    if (!box_intersection(&wuss->scr->clip, icon_box, &clipped.clip))
    {
      x = icon_box->x0 +
          ((icon_box->x1 - icon_box->x0) - icon->spec.image->size.w) / 2;
      y = icon_box->y0 +
          ((icon_box->y1 - icon_box->y0) - icon->spec.image->size.h) / 2;
      screen_copy_bitmap(&clipped, x, y, icon->spec.image);
    }
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

  for (i = 0; i < wuss->iconbar.n; i++)
  {
    box_t slot, icon_box, clipped;

    wuss__iconbar_slot_box(wuss, i, &slot);
    if (slot.x0 >= wuss->scr->size.w)
      continue;

    if (box_intersection(piece, &slot, &clipped))
      continue;

    /* earlier drawing (client redraw, window icons) may have changed it */
    wuss->scr->clip = *piece;

    wuss__iconbar_icon_box(wuss, i, &icon_box);
    iconbar_draw_icon(wuss, wuss->iconbar.icons[i], &icon_box);
  }
}

#endif /* WUSS_ICONBAR */
