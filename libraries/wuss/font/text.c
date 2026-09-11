/* wuss/font/text.c -- wuss centralised bitmap-font text rendering */

#include "geom/point.h"
#include "framebuf/bmfont.h"
#include "framebuf/screen.h"

#include "font.h"

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
  return bmfont_draw(font, scr, text, len, fg, bg, pos, end_pos);
}
