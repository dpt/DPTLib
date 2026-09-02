/* text/bmtext/draw.c -- draw pre-laid-out bitmap-font lines */

#include <stddef.h>

#include "framebuf/bmfont.h"
#include "framebuf/screen.h"
#include "geom/point.h"

#include "text/bmtext.h"

void bmtext_draw(bmfont_t            *font,
                 screen_t            *scr,
                 const bmtext_line_t *lines,
                 int                  nlines,
                 colour_t             fg,
                 colour_t             bg,
                 int                  leading,
                 point_t              origin)
{
  int     font_width, font_height;
  point_t pos;
  int     i;

  bmfont_get_info(font, &font_width, &font_height);

  pos = origin;
  for (i = 0; i < nlines; i++)
  {
    bmfont_draw(font, scr, lines[i].str, lines[i].len, fg, bg, &pos, NULL);
    pos.y += font_height + leading;
  }
}
