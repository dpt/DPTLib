/* text/bmtext/layout.c -- break a string into pixel-fitted lines */

#include <ctype.h>
#include <stddef.h>

#include "framebuf/bmfont.h"

#include "text/bmtext.h"

/* ----------------------------------------------------------------------- */

typedef struct bmtext_font_measure
{
  bmfont_t               *font;
  const bmfont_spacing_t *spacing;
}
bmtext_font_measure_t;

static result_t bmtext_measure_font(const char     *text,
                                    int             len,
                                    bmfont_width_t  target_width,
                                    int            *split_point,
                                    bmfont_width_t *actual_width,
                                    void           *opaque)
{
  bmtext_font_measure_t *fm = opaque;

  return bmfont_measure(fm->font, text, len, fm->spacing, target_width,
                        split_point, actual_width);
}

/* ----------------------------------------------------------------------- */

int bmtext_layout_measured(bmtext_measure_fn *measure,
                           void              *opaque,
                           const char        *string,
                           int                stringlen,
                           int                wrap_width,
                           bmtext_line_t     *lines,
                           int                max)
{
  int nlines;

  nlines = 0;

  while (stringlen > 0 && nlines < max)
  {
    int            absolute_break;
    bmfont_width_t width;
    int            friendly_break;
    int            linelen;

    measure(string, stringlen, wrap_width, &absolute_break, &width, opaque);

    friendly_break = absolute_break;
    linelen        = absolute_break;
    if (absolute_break < stringlen)
    {
      /* line didn't fit whole: try to break at the last space within it, or
       * just after it, where a word ends exactly at the wrap width */
      for (friendly_break = absolute_break; friendly_break > 0; friendly_break--)
        if (isspace((unsigned char) string[friendly_break]))
          break;
      if (friendly_break <= 0)
        friendly_break = absolute_break; /* no space to break at: hard break */

      /* the spaces at the break belong to neither line */
      linelen = friendly_break;
      while (linelen > 0 && isspace((unsigned char) string[linelen - 1]))
        linelen--;
    }

    lines[nlines].str = string;
    lines[nlines].len = linelen;
    nlines++;

    string    += friendly_break;
    stringlen -= friendly_break;
    while (stringlen > 0 && isspace((unsigned char) *string))
    {
      string++;
      stringlen--;
    }
  }

  return nlines;
}

int bmtext_layout(bmfont_t               *font,
                  const char             *string,
                  int                     stringlen,
                  int                     wrap_width,
                  const bmfont_spacing_t *spacing,
                  bmtext_line_t          *lines,
                  int                     max)
{
  bmtext_font_measure_t fm;

  fm.font    = font;
  fm.spacing = spacing;

  return bmtext_layout_measured(bmtext_measure_font, &fm, string, stringlen,
                                wrap_width, lines, max);
}
