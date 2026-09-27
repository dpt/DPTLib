/* wuss/font/text.c -- wuss centralised bitmap-font text rendering */

#include <limits.h>
#include <string.h>

#include "geom/point.h"
#include "framebuf/bmfont.h"
#include "framebuf/screen.h"

#include "wuss/menu.h"

#include "../core/impl.h"
#include "font.h"

/* ----------------------------------------------------------------------- */

result_t wuss__text_measure(bmfont_t       *font,
                            const char     *text,
                            int             len,
                            bmfont_width_t  target_width,
                            int            *split_point,
                            bmfont_width_t *actual_width)
{
  return bmfont_measure(font, text, len, NULL, target_width, split_point,
                        actual_width);
}

result_t wuss__text_draw(bmfont_t      *font,
                         screen_t      *scr,
                         const char    *text,
                         int            len,
                         colour_t       fg,
                         colour_t       bg,
                         const point_t *pos,
                         point_t       *end_pos)
{
  return bmfont_draw(font, scr, text, len, fg, bg, NULL, pos, end_pos);
}

/* ----------------------------------------------------------------------- */

/* Walk "label" as runs: each WUSS_MENU_SHIFT (from the symbol font, when
 * there is one) and the text between them (from the bold weight). Measures
 * the whole label, and draws it too when "scr" is non-NULL. Adjacent runs in
 * different fonts are held "run_gap" pixels apart: a font's spacing
 * only guarantees separation from its own glyphs, so a symbol whose ink
 * fills its advance would otherwise touch the next letter. */
static bmfont_width_t shortcut_runs(const wuss_t  *wuss,
                                    screen_t      *scr,
                                    const char    *label,
                                    colour_t       fg,
                                    colour_t       bg,
                                    const point_t *pos)
{
  static const int glyph_len = sizeof(WUSS_MENU_SHIFT) - 1;
  static const int run_gap   = 1;

  bmfont_t      *bold;
  bmfont_t      *symbol;
  bmfont_t      *prev;
  bmfont_width_t total;
  point_t        at;
  const char    *run;

  bold   = wuss__bold_font(wuss);
  symbol = wuss->fonts.fonts[WUSS_SYMBOL_FONT];
  prev   = NULL;
  total  = 0;
  at.x   = (pos != NULL) ? pos->x : 0;
  at.y   = (pos != NULL) ? pos->y : 0;

  for (run = label; *run != '\0'; )
  {
    const char    *glyph;
    bmfont_t      *font;
    int            len;
    bmfont_width_t w;

    glyph = (symbol != NULL) ? strstr(run, WUSS_MENU_SHIFT) : NULL;
    if (glyph == run)
    {
      font = symbol;
      len  = glyph_len;
    }
    else
    {
      font = bold;
      len  = (glyph != NULL) ? (int) (glyph - run) : (int) strlen(run);
    }

    if (prev != NULL && prev != font)
    {
      at.x  += run_gap;
      total += run_gap;
    }
    prev = font;

    w = 0;
    wuss__text_measure(font, run, len, INT_MAX, NULL, &w);
    if (scr != NULL)
      wuss__text_draw(font, scr, run, len, fg, bg, &at, NULL);
    at.x  += (int) w;
    total += w;
    run   += len;
  }

  return total;
}

bmfont_width_t wuss__shortcut_measure(const wuss_t *wuss, const char *label)
{
  static const colour_t none = { 0 };

  return shortcut_runs(wuss, NULL, label, none, none, NULL);
}

void wuss__shortcut_draw(const wuss_t  *wuss,
                         screen_t      *scr,
                         const char    *label,
                         colour_t       fg,
                         colour_t       bg,
                         const point_t *pos)
{
  (void) shortcut_runs(wuss, scr, label, fg, bg, pos);
}

/* ----------------------------------------------------------------------- */

result_t wuss_text_measure(const wuss_t   *wuss,
                           int             index,
                           const char     *text,
                           int             len,
                           bmfont_width_t  target_width,
                           int            *split_point,
                           bmfont_width_t *actual_width)
{
  bmfont_t *font;

  font = wuss_get_font_n(wuss, index);
  if (font == NULL)
    return result_WUSS_BAD_INDEX;

  return wuss__text_measure(font, text, len, target_width, split_point,
                            actual_width);
}

result_t wuss_text_draw(const wuss_t  *wuss,
                        int            index,
                        screen_t      *scr,
                        const char    *text,
                        int            len,
                        colour_t       fg,
                        colour_t       bg,
                        const point_t *pos,
                        point_t       *end_pos)
{
  bmfont_t *font;

  font = wuss_get_font_n(wuss, index);
  if (font == NULL)
    return result_WUSS_BAD_INDEX;

  return wuss__text_draw(font, scr, text, len, fg, bg, pos, end_pos);
}
