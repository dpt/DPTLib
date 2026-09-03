/* wuss/icon/draw.c -- draw a work-area icon */

#include <string.h>

#include "base/utils.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"
#include "framebuf/bmfont.h"
#include "framebuf/screen.h"

#include "../impl.h"

/* ----------------------------------------------------------------------- */

/* Shared per-draw state, resolved once by wuss__icon_draw and handed to each
 * type's draw helper. "b" is the icon's screen box; "fg" is its resolved
 * foreground; "font" is the icon's selected weight (falling back to slot 0);
 * "font_height" is 0 and "have_font" is 0 when there is no font or no text to
 * draw. */
typedef struct icon_draw_ctx
{
  wuss_t            *wuss;
  screen_t          *scr;
  const wuss_icon_t *icon;
  bmfont_t          *font;
  box_t              b;
  colour_t           fg;
  int                font_height;
  int                have_font;
}
icon_draw_ctx_t;

/* ----------------------------------------------------------------------- */

static void icon_bevel(screen_t    *scr,
                       const box_t *b,
                       colour_t     fill,
                       colour_t     light,
                       colour_t     dark)
{
  screen_fill_rect(scr, b->x0, b->y0,
                   SIZE2D(b->x1 - b->x0, b->y1 - b->y0), fill);

  screen_draw_line(scr, b->x0,     b->y0,     b->x1 - 1, b->y0,     light);
  screen_draw_line(scr, b->x0,     b->y0,     b->x0,     b->y1 - 1, light);
  screen_draw_line(scr, b->x0,     b->y1 - 1, b->x1 - 1, b->y1 - 1, dark);
  screen_draw_line(scr, b->x1 - 1, b->y0,     b->x1 - 1, b->y1 - 1, dark);
}

/* Resolve the ground an icon's text/glyph blends against: an explicit icon bg,
 * else the window's dominant backdrop colour (a pattern fill blends against its
 * clear-bit colour, not its foreground), else "fallback" so bmfont still has a
 * blend colour. */
static colour_t icon_blend_ground(const icon_draw_ctx_t *c,
                                  colour_t               fallback)
{
  const wuss_icon_t *icon = c->icon;

  if (icon->bg != wuss_NO_BACKGROUND)
    return c->wuss->palette[icon->bg];

  if (icon->window->bg.colour != wuss_NO_BACKGROUND)
    return (icon->window->bg.pattern != screen_PATTERN_SOLID)
         ? c->wuss->palette[icon->window->bg.pattern_bg]
         : c->wuss->palette[icon->window->bg.colour];

  return fallback;
}

/* ----------------------------------------------------------------------- */

static void wuss__icon_draw_pattern(const icon_draw_ctx_t *c,
                                    const box_t           *content,
                                    point_t                scroll)
{
  const wuss_icon_t *icon = c->icon;
  colour_t           pat_fg;

  /* disabled: fold the pattern into its own ground so it reads as greyed,
   * mirroring the button's fg-swap */
  pat_fg = (icon->flags & wuss_ICON_FLAGS_DISABLED)
         ? c->wuss->palette[icon->bg]
         : c->fg;

  {
    pattern_t pat;

    pat = pattern_from_preset(icon->pattern,
                              pat_fg, c->wuss->palette[icon->bg]);
    pat.origin = POINT(content->x0 - scroll.x, content->y0 - scroll.y);
    screen_fill_pattern(c->scr, &c->b, &pat);
  }
}

/* ----------------------------------------------------------------------- */

static void wuss__icon_draw_label(const icon_draw_ctx_t *c)
{
  const wuss_icon_t *icon = c->icon;
  const box_t       *b    = &c->b;
  colour_t           bg;
  point_t            pos;
  int                interior_w, split_point;
  bmfont_width_t     width;

  if (icon->bg != wuss_NO_BACKGROUND)
  {
    bg = c->wuss->palette[icon->bg];
    screen_fill_rect(c->scr, b->x0, b->y0,
                     SIZE2D(b->x1 - b->x0, b->y1 - b->y0), bg);
  }
  else
  {
    bg = icon_blend_ground(c, c->fg);
  }

  if (!c->have_font)
    return;

  interior_w = (b->x1 - b->x0) - 2;
  if (interior_w < 1)
    interior_w = 1;

  bmfont_measure(c->font, icon->text, (int) strlen(icon->text),
                 interior_w, &split_point, &width);

  if (icon->flags & wuss_ICON_FLAGS_JUSTIFY_CENTRE)
    pos.x = b->x0 + ((b->x1 - b->x0) - width) / 2;
  else if (icon->flags & wuss_ICON_FLAGS_JUSTIFY_RIGHT)
    pos.x = b->x1 - 1 - width;
  else
    pos.x = b->x0 + 1;
  pos.y = b->y0 + (b->y1 - b->y0 - c->font_height) / 2;

  bmfont_draw(c->font, c->scr, icon->text, (int) strlen(icon->text),
              c->fg, bg, &pos, NULL);
}

/* ----------------------------------------------------------------------- */

static void wuss__icon_draw_frame(const icon_draw_ctx_t *c)
{
  const wuss_icon_t *icon = c->icon;
  const box_t       *b    = &c->b;
  colour_t           bg;
  int                cap_w, cap_x, gap_x0, gap_x1, mid_y;

  bg = icon_blend_ground(c, c->fg);

  cap_w = 0;
  if (c->have_font)
  {
    int            split_point;
    bmfont_width_t width;

    bmfont_measure(c->font, icon->text, (int) strlen(icon->text),
                   (b->x1 - b->x0) - WUSS_FRAME_CAPTION_INSET * 2,
                   &split_point, &width);
    cap_w = width;
  }

  mid_y = b->y0 + c->font_height / 2;

  /* left, right and bottom edges are unbroken */
  screen_draw_line(c->scr, b->x0, mid_y, b->x0, b->y1 - 1, c->fg);
  screen_draw_line(c->scr, b->x1 - 1, mid_y, b->x1 - 1, b->y1 - 1, c->fg);
  screen_draw_line(c->scr, b->x0, b->y1 - 1, b->x1 - 1, b->y1 - 1, c->fg);

  /* the top edge is broken around the caption */
  cap_x  = b->x0 + WUSS_FRAME_CAPTION_INSET;
  gap_x0 = cap_x - WUSS_FRAME_CAPTION_PAD;
  gap_x1 = cap_x + cap_w + WUSS_FRAME_CAPTION_PAD;
  if (gap_x0 > b->x0)
    screen_draw_line(c->scr, b->x0, mid_y, gap_x0, mid_y, c->fg);
  if (gap_x1 < b->x1 - 1)
    screen_draw_line(c->scr, gap_x1, mid_y, b->x1 - 1, mid_y, c->fg);

  if (c->have_font && cap_w > 0)
  {
    point_t pos;

    pos.x = cap_x;
    pos.y = b->y0;
    bmfont_draw(c->font, c->scr, icon->text, (int) strlen(icon->text),
                c->fg, bg, &pos, NULL);
  }
}

/* ----------------------------------------------------------------------- */

static void wuss__icon_draw_button(const icon_draw_ctx_t *c)
{
  const wuss_icon_t *icon = c->icon;
  const box_t       *b    = &c->b;
  colour_t           light, dark, base, label;
  int                pressed, is_default;

  pressed    = wuss__icon_pressed(icon);
  is_default = (icon->flags & wuss_ICON_FLAGS_DEFAULT) != 0;

  if (is_default)
  {
    /* default action button: a flat accent-filled rectangle inside a
     * one-pixel accent-text border, distinct from the bevelled ordinary
     * buttons around it */
    base  = c->wuss->palette[c->wuss->accent_bg];
    light = c->wuss->palette[c->wuss->accent_fg];
    dark  = light;
    label = c->wuss->palette[c->wuss->accent_fg];
  }
  else
  {
    base  = c->wuss->palette[icon->bg];
    light = c->wuss->palette[c->wuss->bevel_light];
    dark  = c->wuss->palette[c->wuss->bevel_dark];
    label = c->fg;
  }

  if (icon->flags & wuss_ICON_FLAGS_DISABLED)
    label = c->wuss->palette[c->wuss->bevel_dark]; /* greyed: sink toward dark */

  if (pressed)
    icon_bevel(c->scr, b, base, dark, light);
  else
    icon_bevel(c->scr, b, base, light, dark);

  if (c->have_font)
  {
    point_t        pos;
    int            interior_w, split_point;
    bmfont_width_t width;

    interior_w = (b->x1 - b->x0) - 2;
    if (interior_w < 1)
      interior_w = 1;

    bmfont_measure(c->font, icon->text, (int) strlen(icon->text),
                   interior_w, &split_point, &width);

    pos.x = b->x0 + ((b->x1 - b->x0) - width) / 2;
    pos.y = b->y0 + (b->y1 - b->y0 - c->font_height) / 2;
    if (pressed)
    {
      pos.x += 1;
      pos.y += 1;
    }

    bmfont_draw(c->font, c->scr, icon->text, (int) strlen(icon->text),
                label, base, &pos, NULL);
  }
}

/* ----------------------------------------------------------------------- */

/* wuss_ICON_TYPE_RADIO and wuss_ICON_TYPE_OPTION: a font-height square glyph at
 * the left, vertically centred, with the label to its right. RADIO draws a
 * square ring with a solid centre when selected; OPTION draws a box with a tick
 * when selected. */
static void wuss__icon_draw_radio_option(const icon_draw_ctx_t *c)
{
  const wuss_icon_t *icon = c->icon;
  const box_t       *b    = &c->b;
  colour_t           glyph, bg;
  box_t              g;
  int                gsz, gy, tx;

  gsz = (c->font_height >= 8) ? c->font_height : 8;
  if (gsz > b->y1 - b->y0)
    gsz = b->y1 - b->y0;
  gy = b->y0 + (b->y1 - b->y0 - gsz) / 2;

  g.x0 = b->x0;
  g.y0 = gy;
  g.x1 = b->x0 + gsz;
  g.y1 = gy + gsz;

  glyph = (icon->flags & wuss_ICON_FLAGS_DISABLED)
        ? c->wuss->palette[c->wuss->bevel_dark]
        : c->fg;

  bg = icon_blend_ground(c, glyph);

  if (icon->bg != wuss_NO_BACKGROUND)
    screen_fill_rect(c->scr, b->x0, b->y0,
                     SIZE2D(b->x1 - b->x0, b->y1 - b->y0), bg);

  if (icon->type == wuss_ICON_TYPE_RADIO)
  {
    /* a square ring (no circle primitive); a solid centre when selected */
    screen_draw_line(c->scr, g.x0 + 2, g.y0,     g.x1 - 3, g.y0,     glyph);
    screen_draw_line(c->scr, g.x0 + 2, g.y1 - 1, g.x1 - 3, g.y1 - 1, glyph);
    screen_draw_line(c->scr, g.x0,     g.y0 + 2, g.x0,     g.y1 - 3, glyph);
    screen_draw_line(c->scr, g.x1 - 1, g.y0 + 2, g.x1 - 1, g.y1 - 3, glyph);
    if (wuss__icon_selected(icon))
      screen_fill_rect(c->scr, g.x0 + 3, g.y0 + 3,
                       SIZE2D(gsz - 6, gsz - 6), glyph);
  }
  else
  {
    /* a box; a tick (two strokes) when selected */
    screen_draw_rect(c->scr, g.x0, g.y0,
                             SIZE2D(g.x1 - g.x0, g.y1 - g.y0), glyph);
    if (wuss__icon_selected(icon))
    {
      screen_draw_line(c->scr, g.x0 + 2, g.y0 + gsz / 2,
                       g.x0 + gsz / 2 - 1, g.y1 - 3, glyph);
      screen_draw_line(c->scr, g.x0 + gsz / 2 - 1, g.y1 - 3,
                       g.x1 - 3, g.y0 + 2, glyph);
    }
  }

  if (c->have_font)
  {
    point_t        pos;
    int            interior_w, split_point;
    bmfont_width_t width;

    tx = g.x1 + 4;
    interior_w = (b->x1 - tx) - 1;
    if (interior_w < 1)
      interior_w = 1;

    bmfont_measure(c->font, icon->text, (int) strlen(icon->text),
                   interior_w, &split_point, &width);
    NOT_USED(width);

    pos.x = tx;
    pos.y = b->y0 + (b->y1 - b->y0 - c->font_height) / 2;
    bmfont_draw(c->font, c->scr, icon->text, (int) strlen(icon->text),
                glyph, bg, &pos, NULL);
  }
}

/* ----------------------------------------------------------------------- */

static void wuss__icon_draw_bitmap(const icon_draw_ctx_t *c)
{
  const box_t *b = &c->b;
  screen_t     clipped;

  if (c->icon->bitmap == NULL)
    return;

  /* screen_copy_bitmap clips to scr->clip and does not scale, so narrow the
   * clip to the icon box (intersected with whatever redraw already set) and
   * blit at the box's top-left */
  clipped = *c->scr;
  if (box_intersection(&c->scr->clip, b, &clipped.clip))
    return;

  screen_copy_bitmap(&clipped, b->x0, b->y0, c->icon->bitmap);
}

/* ----------------------------------------------------------------------- */

static void wuss__icon_draw_menu_entry(const icon_draw_ctx_t *c)
{
  const wuss_icon_t *icon = c->icon;
  const box_t       *b    = &c->b;
  colour_t           ink, ground, tmp;
  int                disabled, highlit, pad;

  disabled = (icon->flags & wuss_ICON_FLAGS_DISABLED) != 0;
  highlit  = wuss__icon_hovered(icon) && !disabled;
  pad      = 4;

  /* resolve the row's own ink over its own/inherited ground */
  ground = icon_blend_ground(c, c->fg);
  ink = disabled ? c->wuss->palette[c->wuss->bevel_dark] : c->fg;

  /* highlight simply swaps the row's fg/bg */
  if (highlit)
  {
    tmp    = ink;
    ink    = ground;
    ground = tmp;
  }

  if (highlit || icon->bg != wuss_NO_BACKGROUND)
    screen_fill_rect(c->scr, b->x0, b->y0,
                     SIZE2D(b->x1 - b->x0, b->y1 - b->y0), ground);

  /* left-edge colour chip (wins over the tick) or tick when selected */
  if ((icon->flags & wuss_ICON_FLAGS_SWATCH) &&
      icon->swatch != wuss_NO_BACKGROUND)
  {
    int cx, cy, h;

    h  = (c->font_height >= 8) ? c->font_height : 8;
    cx = b->x0 + pad;
    cy = b->y0 + (b->y1 - b->y0 - h) / 2;
    screen_fill_rect(c->scr, cx, cy, SIZE2D(h, h),
                     c->wuss->palette[icon->swatch]);
    screen_draw_rect(c->scr, cx, cy, SIZE2D(h, h), ink); /* 1px border */
  }
  else if (wuss__icon_selected(icon))
  {
    int cx, cy, h;

    h  = (c->font_height >= 8) ? c->font_height : 8;
    cx = b->x0 + pad;
    cy = b->y0 + (b->y1 - b->y0 - h) / 2;
    screen_draw_line(c->scr, cx, cy + h / 2, cx + h / 2 - 1, cy + h - 2, ink);
    screen_draw_line(c->scr, cx + h / 2 - 1, cy + h - 2, cx + h - 2, cy, ink);
  }

  /* right-edge arrow for a submenu entry */
  if (icon->flags & wuss_ICON_FLAGS_SUBMENU)
  {
    int ax, ay, r, dy;

    r  = (c->font_height >= 8) ? c->font_height / 3 : 3;
    ax = b->x1 - 1 - pad - r;
    ay = b->y0 + (b->y1 - b->y0) / 2;
    for (dy = -r; dy <= r; dy++)
      screen_draw_line(c->scr, ax, ay + dy,
                       ax + (r - (dy < 0 ? -dy : dy)), ay + dy, ink);
  }

  if (c->have_font && icon->text != NULL && icon->text[0] != '\0')
  {
    point_t pos;

    pos.x = b->x0 + pad + c->font_height; /* leave room for a tick */
    pos.y = b->y0 + (b->y1 - b->y0 - c->font_height) / 2;
    bmfont_draw(c->font, c->scr, icon->text, (int) strlen(icon->text),
                ink, ground, &pos, NULL);
  }
}

/* ----------------------------------------------------------------------- */

/* wuss_ICON_TYPE_RULE: a dashed rule centred in the box, drawn in the icon's
 * fg over whatever ground it inherits. Inert -- see wuss__icon_hit_test. */
static void wuss__icon_draw_rule(const icon_draw_ctx_t *c)
{
  const box_t *b = &c->b;
  int          mid_y;

  mid_y = b->y0 + (b->y1 - b->y0) / 2;
  screen_draw_dashed_line(c->scr,
                          b->x0, mid_y, b->x1 - 1, mid_y,
                          2, 2, c->fg);
}

/* ----------------------------------------------------------------------- */

void wuss__icon_draw(wuss_t            *wuss,
                     const wuss_icon_t *icon,
                     const box_t       *content,
                     point_t            scroll)
{
  icon_draw_ctx_t c;
  int             font_width;
  int             fontidx;

  if (icon->flags & wuss_ICON_FLAGS_HIDDEN)
    return;

  wuss__icon_box_to_screen(content, scroll, &icon->bbox, &c.b);

  if (box_is_empty(&c.b))
    return;

  c.wuss = wuss;
  c.scr  = wuss->scr;
  c.icon = icon;
  c.fg   = wuss->palette[icon->fg];

  /* pick the icon's requested weight; fall back to the system font */
  fontidx = wuss_ICON_FONT_OF(icon->flags);
  c.font  = wuss->fonts[fontidx];
  if (c.font == NULL)
    c.font = wuss->fonts[0];

  c.have_font = (c.font != NULL && icon->text[0] != '\0');
  if (c.have_font)
    bmfont_get_info(c.font, &font_width, &c.font_height);
  else
    font_width = c.font_height = 0;
  NOT_USED(font_width);

  switch (icon->type)
  {
  case wuss_ICON_TYPE_PATTERN:
    wuss__icon_draw_pattern(&c, content, scroll);
    break;

  case wuss_ICON_TYPE_LABEL:
    wuss__icon_draw_label(&c);
    break;

  case wuss_ICON_TYPE_FRAME:
    wuss__icon_draw_frame(&c);
    break;

  case wuss_ICON_TYPE_BUTTON:
    wuss__icon_draw_button(&c);
    break;

  case wuss_ICON_TYPE_RADIO:
  case wuss_ICON_TYPE_OPTION:
    wuss__icon_draw_radio_option(&c);
    break;

  case wuss_ICON_TYPE_BITMAP:
    wuss__icon_draw_bitmap(&c);
    break;

  case wuss_ICON_TYPE_MENU_ENTRY:
    wuss__icon_draw_menu_entry(&c);
    break;

  case wuss_ICON_TYPE_RULE:
    wuss__icon_draw_rule(&c);
    break;
  }
}
