/* wuss/icon/draw.c -- draw a work-area icon */

#include <limits.h>
#include <string.h>

#include "base/utils.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"
#include "framebuf/bmfont.h"
#include "framebuf/colour.h"
#include "framebuf/pattern.h"
#include "framebuf/screen.h"

#include "../core/impl.h"

/* ----------------------------------------------------------------------- */

/* Shared per-draw state, resolved once by wuss__icon_draw and handed to each
 * type's draw helper. "b" is the icon's screen box; "fg" is its resolved
 * foreground; "font" is the icon's selected weight (falling back to slot 0);
 * "font_height", "font_ascent" and "have_font" are 0 when there is no font or
 * no text to draw. Draw helpers vertically centre text via
 * icon_text_baseline_y(). */
typedef struct icon_draw_ctx
{
  wuss_t              *wuss;
  const wuss_window_t *window;
  screen_t            *scr;
  const wuss_icon_t   *icon;
  bmfont_t            *font;
  box_t                b;
  colour_t             fg;
  int                  font_height;
  int                  font_ascent;
  int                  have_font;
}
icon_draw_ctx_t;

/* Baseline y for text centred by ascender alone (not the descender too, so a
 * run of ascender-only text -- e.g. all caps and digits -- sits visually
 * centred rather than pulled high by unused descender space) in box "b". */
static int icon_text_baseline_y(const icon_draw_ctx_t *c, const box_t *b)
{
  return b->y0 + (b->y1 - b->y0 - c->font_ascent) / 2 + c->font_ascent;
}

/* ----------------------------------------------------------------------- */

static void icon_fill_bevel(screen_t    *scr,
                            const box_t *b,
                            colour_t     fill,
                            colour_t     light,
                            colour_t     dark)
{
  screen_fill_rect(scr, b->x0, b->y0, box_size(b), fill);
  screen_draw_bevel_edge(scr, b, light, dark);
}

/* The 6px-per-edge "action" surround shared by wuss_ICON_BORDER_ACTION and a
 * default button: a 2px sunken outset (dark top/left), a 2px accent moat, then
 * a 2px raised inset. Only the inset tracks the pressed state -- it flips to
 * sunken -- so a pressed action button reads as pushed in without the whole
 * surround inverting. */
static void icon_draw_action_border(screen_t    *scr,
                                    const box_t *b,
                                    colour_t     light,
                                    colour_t     dark,
                                    colour_t     accent,
                                    int          pressed)
{
  box_t ring;

  ring = *b;
  screen_draw_bevel_edge(scr, &ring, dark, light);

  ring = box_grown(b, -2);
  screen_draw_bevel_edge(scr, &ring, accent, accent);

  ring = box_grown(b, -4);
  screen_draw_bevel_edge(scr, &ring, pressed ? dark : light,
                                     pressed ? light : dark);
}

/* The wuss_ICON_BORDER_DIVIDER surround: two 2px bevels in a lighter pair
 * than RIDGE/GROOVE -- bevel_divider against bevel_light rather than the full
 * light/dark contrast. Outer ring sunken, inner ring raised. Shared by the
 * bordered label and the grouping frame. */
static void icon_draw_divider_border(screen_t    *scr,
                                     const box_t *b,
                                     colour_t     light,
                                     colour_t     divider)
{
  box_t ring;

  ring = *b;
  screen_draw_bevel_edge(scr, &ring, divider, light);

  ring = box_grown(b, -2);
  screen_draw_bevel_edge(scr, &ring, light, divider);
}

/* Resolve the ground an icon's text/glyph blends against: an explicit icon bg,
 * else the window's dominant backdrop colour (a pattern fill blends against its
 * clear-bit colour, not its foreground), else "fallback" so bmfont still has a
 * blend colour. */
static colour_t icon_blend_ground(const icon_draw_ctx_t *c,
                                  colour_t               fallback)
{
  const wuss_icon_spec_t *icon = &c->icon->spec;

  if (icon->bg != wuss_NO_BACKGROUND)
    return c->wuss->palette[icon->bg];

  if (c->window->bg.colour != wuss_NO_BACKGROUND)
    return (c->window->bg.pattern != screen_PATTERN_SOLID)
         ? c->wuss->palette[c->window->bg.pattern_bg]
         : c->wuss->palette[c->window->bg.colour];

  return fallback;
}

/* Draw one menu-decoration glyph (a tick or a submenu arrow) from the symbol
 * font slot, centred on "centre", blending "ink" over "ground". Returns 1 when
 * the glyph was drawn, 0 when the slot is empty or the font lacks that cell --
 * the caller then falls back to its vector rendering. */
static int wuss__draw_symbol_glyph(const wuss_t *wuss,
                                   screen_t     *scr,
                                   point_t       centre,
                                   char          glyph,
                                   colour_t      ink,
                                   colour_t      ground)
{
  bmfont_t *font;
  point_t   pos;
  int       font_width, font_height, font_ascent;

  font = wuss->fonts.fonts[WUSS_SYMBOL_FONT];
  if (font == NULL)
    return 0;

  if ((unsigned char) glyph < ' ' ||
      (unsigned char) glyph - ' ' >= bmfont_get_count(font))
    return 0;

  bmfont_get_info(font, &font_width, &font_height, &font_ascent, NULL);
  pos.x = centre.x - font_width / 2;
  pos.y = centre.y - font_height / 2 + font_ascent;
  bmfont_draw(font, scr, &glyph, 1, ink, ground, NULL, &pos, NULL);
  return 1;
}

/* ----------------------------------------------------------------------- */

static void wuss__icon_draw_pattern(const icon_draw_ctx_t *c,
                                    const box_t           *content,
                                    point_t                scroll)
{
  const wuss_icon_spec_t *icon = &c->icon->spec;
  colour_t                pat_fg;

  /* disabled: fold the pattern into its own ground so it reads as greyed,
   * mirroring the button's fg-swap */
  pat_fg = (icon->flags & wuss_ICON_FLAGS_DISABLED)
         ? c->wuss->palette[icon->bg]
         : c->fg;

  {
    pattern_t pat;

    pat = pattern_from_preset(icon->u.pattern.tile,
                              pat_fg, c->wuss->palette[icon->bg]);
    pat.origin = POINT(content->x0 - scroll.x, content->y0 - scroll.y);
    screen_fill_pattern(c->scr, &c->b, &pat);
  }
}

/* ----------------------------------------------------------------------- */

static void wuss__icon_draw_label(const icon_draw_ctx_t *c)
{
  const wuss_icon_spec_t *icon = &c->icon->spec;
  const box_t            *b    = &c->b;
  colour_t                bg;
  point_t                 pos;
  bmfont_width_t          width;
  int                     len;

  if (icon->bg != wuss_NO_BACKGROUND)
  {
    bg = c->wuss->palette[icon->bg];
    screen_fill_rect(c->scr, b->x0, b->y0, box_size(b), bg);
  }
  else
  {
    bg = icon_blend_ground(c, c->fg);
  }

  if (icon->u.label.border != wuss_ICON_BORDER_NONE)
  {
    colour_t light, dark, accent, divider;

    light   = c->wuss->palette[c->wuss->bevel_light];
    dark    = c->wuss->palette[c->wuss->bevel_dark];
    accent  = c->wuss->palette[c->wuss->accent]; /* the action-button fill */
    divider = c->wuss->palette[c->wuss->bevel_divider];

    if (icon->u.label.border == wuss_ICON_BORDER_ACTION)
    {
      icon_draw_action_border(c->scr, b, light, dark, accent, 0);
    }
    else if (icon->u.label.border == wuss_ICON_BORDER_DIVIDER)
    {
      icon_draw_divider_border(c->scr, b, light, divider);
    }
    else
    {
      /* RIDGE reads raised (light top/left); GROOVE reads sunken. One 2px
       * bevel ring. */
      box_t ring = *b;

      if (icon->u.label.border == wuss_ICON_BORDER_RIDGE)
        screen_draw_bevel_edge(c->scr, &ring, light, dark);
      else
        screen_draw_bevel_edge(c->scr, &ring, dark, light);
    }
  }

  if (!c->have_font)
    return;

  /* Measure the whole string, not the box interior: a clipped measurement
   * left the widest label of a right-justified set overhanging the rest. */
  len = (int) strlen(icon->text);
  wuss__text_measure(c->font, icon->text, len, INT_MAX, NULL, &width);

  if (icon->flags & wuss_ICON_FLAGS_JUSTIFY_CENTRE)
    pos.x = b->x0 + ((b->x1 - b->x0) - width) / 2;
  else if (icon->flags & wuss_ICON_FLAGS_JUSTIFY_RIGHT)
    pos.x = b->x1 - 1 - width;
  else
    pos.x = b->x0 + 1;

  pos.y = icon_text_baseline_y(c, b);

  wuss__text_draw(c->font, c->scr, icon->text, len, c->fg, bg, &pos, NULL);
}

/* ----------------------------------------------------------------------- */

static void wuss__icon_draw_frame(const icon_draw_ctx_t *c)
{
  const wuss_icon_spec_t *spec = &c->icon->spec;
  const box_t            *b    = &c->b;
  colour_t                light, divider;
  bmfont_width_t          cap_w;
  int                     len;
  box_t                   frame;
  colour_t                bg;
  point_t                 pos;

  light   = c->wuss->palette[c->wuss->bevel_light];
  divider = c->wuss->palette[c->wuss->bevel_divider];

  cap_w = 0;
  if (c->have_font)
  {
    len = (int) strlen(spec->text);
    wuss__text_measure(c->font, spec->text, len,
                       (b->x1 - b->x0) - WUSS_FRAME_CAPTION_INSET * 2,
                       NULL, &cap_w);
  }

  frame    = c->b;
  frame.y0 += WUSS_FRAME_CAPTION_TOP;
  icon_draw_divider_border(c->scr, &frame, light, divider);

  if (cap_w > 0)
  {
    bg = icon_blend_ground(c, c->fg);
    pos.x = b->x0 + WUSS_FRAME_CAPTION_INSET;
    pos.y = b->y0 + c->font_ascent;
    wuss__text_draw(c->font, c->scr, spec->text, len, c->fg, bg, &pos, NULL);
  }
}

/* ----------------------------------------------------------------------- */

static void wuss__icon_draw_button(const icon_draw_ctx_t *c)
{
  const wuss_icon_spec_t *icon = &c->icon->spec;
  const box_t            *b    = &c->b;
  colour_t                light, dark, base, label;
  int                     pressed, is_default;

  pressed    = wuss__icon_pressed(c->icon);
  is_default = (icon->flags & wuss_ICON_FLAGS_DEFAULT) != 0;

  /* a plain button whose spec left bg as wuss_NO_BACKGROUND takes the config
   * button face */
  base  = (icon->bg != wuss_NO_BACKGROUND)
        ? c->wuss->palette[icon->bg]
        : c->wuss->palette[c->wuss->button_bg];
  light = c->wuss->palette[c->wuss->bevel_light];
  dark  = c->wuss->palette[c->wuss->bevel_dark];
  /* a default action button labels in button.fg to read against its accent
   * fill; a plain button uses the icon's own fg */
  label = is_default ? c->wuss->palette[c->wuss->button_fg] : c->fg;

  /* a held button fills with the pressed shade regardless of type */
  if (pressed)
    base = c->wuss->palette[c->wuss->button_pressed];

  if (icon->flags & wuss_ICON_FLAGS_DISABLED)
    label = c->wuss->palette[c->wuss->bevel_dark]; /* greyed: sink toward dark */

  if (is_default)
  {
    /* default action button: an accent-filled rectangle inside the same 6px
     * "action" surround as wuss_ICON_BORDER_ACTION, distinct from the plain
     * bevelled buttons around it. */

    screen_fill_rect(c->scr, b->x0, b->y0, box_size(b), base);
    icon_draw_action_border(c->scr, b, light, dark,
                            c->wuss->palette[c->wuss->accent], pressed);
  }
  else
  {
    icon_fill_bevel(c->scr, b, base, pressed ? dark : light,
                    pressed ? light : dark);
  }

  if (c->have_font)
  {
    point_t        pos;
    int            interior_w, split_point, len;
    bmfont_width_t width;

    interior_w = MAX((b->x1 - b->x0) - 2, 1);
    len        = (int) strlen(icon->text);

    wuss__text_measure(c->font, icon->text, len, interior_w, &split_point,
                       &width);

    pos.x = b->x0 + ((b->x1 - b->x0) - width) / 2;
    pos.y = icon_text_baseline_y(c, b);
    if (pressed)
    {
      pos.x += 1;
      pos.y += 1;
    }

    wuss__text_draw(c->font, c->scr, icon->text, len, label, base, &pos,
                    NULL);
  }
}

/* ----------------------------------------------------------------------- */

/* The icon-set bitmap for a radio/option's current state, or NULL when the set
 * is absent or lacks that entry. Names are radon/radoff for RADIO and
 * opton/optoff for OPTION. */
static const bitmap_t *wuss__icon_radio_option_bitmap(const icon_draw_ctx_t *c)
{
  const char *name;
  int         idx;

  if (c->icon->spec.type == wuss_ICON_TYPE_RADIO)
    name = wuss__icon_selected(c->icon) ? "radon" : "radoff";
  else
    name = wuss__icon_selected(c->icon) ? "opton" : "optoff";

  idx = wuss_icons_lookup(c->wuss, name);
  if (idx < 0)
    return NULL;

  return wuss_icons_bitmap(c->wuss, idx);
}

/* wuss_ICON_TYPE_RADIO and wuss_ICON_TYPE_OPTION: a glyph at the left,
 * vertically centred, with the label to its right. If the icon set carries
 * radon/radoff (RADIO) or opton/optoff (OPTION) those bitmaps are blitted for
 * the state, and the glyph box and label offset follow the bitmap's own size;
 * otherwise a font-height square is used -- RADIO draws a square ring with a
 * solid centre when selected and OPTION draws a box with a tick when
 * selected. */
static void wuss__icon_draw_radio_option(const icon_draw_ctx_t *c)
{
  const wuss_icon_spec_t *icon = &c->icon->spec;
  const box_t            *b    = &c->b;
  const bitmap_t         *bm;
  colour_t                glyph, bg;
  box_t                   g;
  int                     gw, gh, gy, tx;

  bm = wuss__icon_radio_option_bitmap(c);

  gw = bm ? bm->size.w : CLAMP(c->font_height, 8, b->x1 - b->x0);
  gh = bm ? bm->size.h : CLAMP(c->font_height, 8, b->y1 - b->y0);
  gy = b->y0 + (b->y1 - b->y0 - gh) / 2;

  g.x0 = b->x0;
  g.y0 = gy;
  g.x1 = b->x0 + gw;
  g.y1 = gy + gh;

  glyph = (icon->flags & wuss_ICON_FLAGS_DISABLED)
        ? c->wuss->palette[c->wuss->bevel_dark]
        : c->fg;

  bg = icon_blend_ground(c, glyph);

  if (icon->bg != wuss_NO_BACKGROUND)
    screen_fill_rect(c->scr, b->x0, b->y0, box_size(b), bg);
  else
    /* no explicit bg to repaint the whole box, but the state we are about to
     * draw (a smaller radon/radoff swap, or a cleared tick/centre) can leave
     * fewer pixels than last time -- clear just the glyph box so no stale
     * glyph shows through. The label to its right never changes on select. */
    screen_fill_rect(c->scr, g.x0, g.y0, box_size(&g), bg);

  if (bm != NULL)
  {
    screen_copy_bitmap(c->scr, g.x0, g.y0, bm);
  }
  else if (icon->type == wuss_ICON_TYPE_RADIO)
  {
    /* a square ring (no circle primitive); a solid centre when selected */
    screen_draw_line(c->scr, g.x0 + 2, g.y0,     g.x1 - 3, g.y0,     glyph);
    screen_draw_line(c->scr, g.x0 + 2, g.y1 - 1, g.x1 - 3, g.y1 - 1, glyph);
    screen_draw_line(c->scr, g.x0,     g.y0 + 2, g.x0,     g.y1 - 3, glyph);
    screen_draw_line(c->scr, g.x1 - 1, g.y0 + 2, g.x1 - 1, g.y1 - 3, glyph);
    if (wuss__icon_selected(c->icon))
      screen_fill_rect(c->scr, g.x0 + 3, g.y0 + 3,
                       SIZE2D(gw - 6, gh - 6), glyph);
  }
  else
  {
    /* a box; a tick (two strokes) when selected */
    screen_draw_rect(c->scr, g.x0, g.y0, box_size(&g), glyph);
    if (wuss__icon_selected(c->icon))
    {
      screen_draw_line(c->scr, g.x0 + 2, g.y0 + gh / 2,
                       g.x0 + gw / 2 - 1, g.y1 - 3, glyph);
      screen_draw_line(c->scr, g.x0 + gw / 2 - 1, g.y1 - 3,
                       g.x1 - 3, g.y0 + 2, glyph);
    }
  }

  if (c->have_font)
  {
    point_t        pos;
    int            interior_w, split_point, len;
    bmfont_width_t width;

    tx = g.x1 + 4;
    interior_w = MAX((b->x1 - tx) - 1, 1);
    len        = (int) strlen(icon->text);

    wuss__text_measure(c->font, icon->text, len, interior_w, &split_point,
                       &width);
    NOT_USED(width);

    pos.x = tx;
    pos.y = icon_text_baseline_y(c, b);
    wuss__text_draw(c->font, c->scr, icon->text, len, glyph, bg, &pos, NULL);
  }
}

/* ----------------------------------------------------------------------- */

static void wuss__icon_draw_bitmap(const icon_draw_ctx_t *c)
{
  const box_t *b = &c->b;
  screen_t     clipped;

  if (c->icon->spec.u.bitmap.image == NULL)
    return;

  /* screen_copy_bitmap clips to scr->clip and does not scale, so narrow the
   * clip to the icon box (intersected with whatever redraw already set) and
   * blit at the box's top-left */
  clipped = *c->scr;
  if (box_intersection(&c->scr->clip, b, &clipped.clip))
    return;

  screen_copy_bitmap(&clipped, b->x0, b->y0, c->icon->spec.u.bitmap.image);
}

/* ----------------------------------------------------------------------- */

static void wuss__icon_draw_menu_entry(const icon_draw_ctx_t *c)
{
  const wuss_icon_spec_t *icon = &c->icon->spec;
  const box_t            *b    = &c->b;
  colour_t                ink, ground, text_ink, text_ground, tmp;
  int                     disabled, highlit, pad, text_x0, text_x1;

  disabled = (icon->flags & wuss_ICON_FLAGS_DISABLED) != 0;
  highlit  = wuss__icon_hovered(c->icon) && !disabled;
  pad      = 4;

  /* resolve the row's own ink over its own/inherited ground */
  ground = icon_blend_ground(c, c->fg);
  ink = disabled ? c->wuss->palette[c->wuss->bevel_dark] : c->fg;

  /* the highlight only swaps fg/bg over the text column -- the tick and
   * submenu-arrow gutters keep the row's normal ink/ground throughout */
  text_ink    = ink;
  text_ground = ground;
  if (highlit)
  {
    tmp         = text_ink;
    text_ink    = text_ground;
    text_ground = tmp;
  }

  text_x0 = b->x0 + pad + c->font_height;      /* past the tick gutter */
  text_x1 = b->x1 - pad - MAX(c->font_height, 8); /* short of the arrow
                                                    * gutter, whether or not
                                                    * this row has an arrow */

  if (highlit)
    screen_fill_rect(c->scr, text_x0, b->y0,
                     SIZE2D(text_x1 - text_x0, b->y1 - b->y0), text_ground);
  else if (icon->bg != wuss_NO_BACKGROUND)
    screen_fill_rect(c->scr, b->x0, b->y0, box_size(b), ground);

  /* left-edge colour chip (wins over the tick) or tick when selected */
  if (icon->flags & wuss_ICON_FLAGS_SWATCH)
  {
    int cx, cy, h;

    h  = MAX(c->font_height, 8);
    cx = b->x0 + pad;
    cy = b->y0 + (b->y1 - b->y0 - h) / 2;
    if (icon->u.menu_entry.swatch == wuss_NO_BACKGROUND)
    {
      /* "None" row: hatched black/white chip, no real palette colour */
      pattern_t hatch;
      box_t     chip;

      chip.x0 = cx;
      chip.y0 = cy;
      chip.x1 = cx + h;
      chip.y1 = cy + h;
      hatch = pattern_from_preset(screen_PATTERN_DIAGONAL,
                                  colour_rgb(0, 0, 0),
                                  colour_rgb(255, 255, 255));
      /* phase against the chip's own corner, not the screen origin the
       * preset defaults to -- otherwise a window-move fast path that
       * blits already-settled pixels to their new position (leaving
       * their old screen-relative phase baked in) meets a freshly
       * repainted sliver phased against the new position, and the two
       * halves' hatching no longer lines up at the seam */
      hatch.origin = POINT(chip.x0, chip.y0);
      screen_fill_pattern(c->scr, &chip, &hatch);
    }
    else
      screen_fill_rect(c->scr, cx, cy, SIZE2D(h, h),
                       c->wuss->palette[icon->u.menu_entry.swatch]);
    screen_draw_rect(c->scr, cx, cy, SIZE2D(h, h), ink); /* 1px border */
  }
  else if (wuss__icon_selected(c->icon))
  {
    int     cx, cy, h;
    point_t centre;

    h  = MAX(c->font_height, 8);
    cx = b->x0 + pad;
    cy = b->y0 + (b->y1 - b->y0 - h) / 2;
    centre.x = cx + h / 2;
    centre.y = cy + h / 2;
    if (!wuss__draw_symbol_glyph(c->wuss, c->scr, centre,
                                 WUSS_GLYPH_TICK, ink, ground))
    {
      screen_draw_line(c->scr, cx, cy + h / 2, cx + h / 2 - 1, cy + h - 2, ink);
      screen_draw_line(c->scr, cx + h / 2 - 1, cy + h - 2, cx + h - 2, cy, ink);
    }
  }

  /* right-edge arrow for a submenu entry */
  if (icon->flags & wuss_ICON_FLAGS_SUBMENU)
  {
    int     ax, ay, r, dy;
    point_t centre;

    r  = (c->font_height >= 8) ? c->font_height / 3 : 3;
    ax = b->x1 - 1 - pad - r;
    ay = b->y0 + (b->y1 - b->y0) / 2;
    centre.x = ax + r / 2;
    centre.y = ay;
    if (!wuss__draw_symbol_glyph(c->wuss, c->scr, centre,
                                 WUSS_GLYPH_SUBMENU, ink, ground))
    {
      for (dy = -r; dy <= r; dy++)
        screen_draw_line(c->scr, ax, ay + dy,
                         ax + (r - (dy < 0 ? -dy : dy)), ay + dy, ink);
    }
  }

  if (c->have_font && icon->text != NULL && icon->text[0] != '\0')
  {
    point_t        pos;
    bmfont_width_t space_w;

    /* draw as if a space padded the text either side, without actually
     * touching the string -- only the left inset matters for pos.x, but
     * the same width is left spare at text_x1 too since the fill already
     * spans the full column */
    space_w = 0;
    wuss__text_measure(c->font, " ", 1, INT_MAX, NULL, &space_w);

    pos.x = text_x0 + (int) space_w;
    pos.y = icon_text_baseline_y(c, b);
    wuss__text_draw(c->font, c->scr, icon->text, (int) strlen(icon->text),
                    text_ink, text_ground, &pos, NULL);
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

static void wuss__icon_draw_slider(const icon_draw_ctx_t *c)
{
  const wuss_icon_spec_t *icon = &c->icon->spec;
  colour_t                surround, track, value, light, dark;
  box_t                   surround_box, groove, fill;
  int                     lo, hi, px;

  surround = c->wuss->palette[c->wuss->slider_surround];
  track    = c->wuss->palette[c->wuss->slider_track];
  value    = c->wuss->palette[c->wuss->slider_value];
  light    = c->wuss->palette[c->wuss->bevel_light];
  dark     = c->wuss->palette[c->wuss->bevel_dark];

  /* draw the surround */
  
  surround_box = box_grown(&c->b, -2); /* inset by bevel width to avoid overdraw */
  screen_fill_rect(c->scr, surround_box.x0, surround_box.y0,
                   box_size(&surround_box), surround);

  /* draw the bevel */
  
  screen_draw_bevel_edge(c->scr, &c->b, dark, light);

  wuss__slider_groove_box(&surround_box, &groove);
  
  /* draw the track */

  lo = MIN(icon->u.slider.min, icon->u.slider.max);
  hi = MAX(icon->u.slider.min, icon->u.slider.max);
  px = wuss__slider_value_to_px(&groove, icon->u.slider.orientation,
                                c->icon->value, lo, hi);
  if (icon->u.slider.min > icon->u.slider.max)
    px = (icon->u.slider.orientation == wuss_SLIDER_HORIZONTAL)
       ? (groove.x1 - groove.x0) - px
       : (groove.y1 - groove.y0) - px;

  fill = groove;
  if (icon->u.slider.orientation == wuss_SLIDER_HORIZONTAL)
    fill.x1 = fill.x0 + px;
  else
    fill.y0 = fill.y1 - px;

  screen_fill_rect(c->scr, groove.x0, groove.y0, box_size(&groove), track);
  if (px > 0)
    screen_fill_rect(c->scr, fill.x0, fill.y0, box_size(&fill), value);
}

/* ----------------------------------------------------------------------- */

void wuss__icon_draw(wuss_t              *wuss,
                     const wuss_window_t *window,
                     const wuss_icon_t   *icon,
                     const box_t         *content,
                     point_t              scroll)
{
  const wuss_icon_spec_t *spec = &icon->spec;
  icon_draw_ctx_t         c;
  int                     fontidx;

  if (spec->flags & wuss_ICON_FLAGS_HIDDEN)
    return;

  wuss__icon_box_to_screen(content, scroll, &spec->bbox, &c.b);

  if (box_is_empty(&c.b))
    return;

  c.wuss   = wuss;
  c.window = window;
  c.scr    = wuss->scr;
  c.icon   = icon;
  c.fg     = wuss->palette[spec->fg];

  /* pick the icon's requested weight; fall back to the system font */
  fontidx = wuss_ICON_FONT_OF(spec->flags);
  c.font  = wuss->fonts.fonts[fontidx];
  if (c.font == NULL)
    c.font = wuss->fonts.fonts[0];

  c.have_font = (c.font != NULL && spec->text[0] != '\0');
  if (c.have_font)
    bmfont_get_info(c.font, NULL, &c.font_height, &c.font_ascent, NULL);
  else
  {
    c.font_height = 0;
    c.font_ascent = 0;
  }

  switch (spec->type)
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

  case wuss_ICON_TYPE_ACTION:
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

  case wuss_ICON_TYPE_SLIDER:
    wuss__icon_draw_slider(&c);
    break;

  /* reserved types with no renderer yet: fall back to a plain label */
  case wuss_ICON_TYPE_DISPLAY:
  case wuss_ICON_TYPE_WRITABLE:
  case wuss_ICON_TYPE_NUMBER:
  case wuss_ICON_TYPE_STRING_SET:
  case wuss_ICON_TYPE_DRAGGABLE:
    wuss__icon_draw_label(&c);
    break;
  }
}
