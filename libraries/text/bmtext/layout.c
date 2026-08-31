/* layout.c -- break a string into pixel-fitted lines */

#include <ctype.h>

#include "framebuf/bmfont.h"

#include "text/bmtext.h"

int bmtext_layout(bmfont_t      *font,
                  const char    *string,
                  int            stringlen,
                  int            wrap_width,
                  bmtext_line_t *lines,
                  int            max)
{
  int nlines;

  nlines = 0;

  while (stringlen > 0 && nlines < max)
  {
    int            absolute_break;
    bmfont_width_t width;
    int            friendly_break;

    bmfont_measure(font, string, stringlen, wrap_width, &absolute_break, &width);

    friendly_break = absolute_break;
    if (absolute_break < stringlen)
    {
      /* line didn't fit whole: try to break at the last space within it */
      for (friendly_break = absolute_break - 1; friendly_break > 0; friendly_break--)
        if (isspace((unsigned char) string[friendly_break]))
          break;
      if (friendly_break <= 0)
        friendly_break = absolute_break; /* no space to break at: hard break */
    }

    lines[nlines].str = string;
    lines[nlines].len = friendly_break;
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
