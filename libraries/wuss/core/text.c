/* text.c -- wuss centralised bitmap-font text rendering */

#include "geom/point.h"
#include "framebuf/bmfont.h"
#include "framebuf/screen.h"

#include "impl.h"

/* Optical vertical bias applied to every text draw so glyphs sit visually
 * centred rather than mathematically centred. Positive shifts text down. */
#ifndef WUSS_TEXT_BASELINE_ADJUST
#define WUSS_TEXT_BASELINE_ADJUST 1
#endif

/* ----------------------------------------------------------------------- */

result_t wuss__text_measure(bmfont_t       *font,
                            const char     *text,
                            int             len,
                            bmfont_width_t  target_width,
                            int            *split_point,
                            bmfont_width_t *actual_width)
{
  return bmfont_measure(font, text, len, target_width, split_point,
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
  point_t adjusted;

  adjusted.x = pos->x;
  adjusted.y = pos->y + WUSS_TEXT_BASELINE_ADJUST;

  return bmfont_draw(font, scr, text, len, fg, bg, &adjusted, end_pos);
}
