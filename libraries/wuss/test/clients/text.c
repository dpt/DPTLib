/* text.c -- wuss test - static paragraph client */

#ifdef USE_SDL

#include <ctype.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "geom/point.h"

#include "text.h"

static const char paragraph[] =
  "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
  "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";

result_t text_redraw(wuss_window_t *window, screen_t *scr, const box_t *content, void *client_data)
{
  text_client_t *tcx;
  int             font_width, font_height;
  const char     *string;
  int             stringlen;
  point_t         pos;

  NOT_USED(window);

  tcx = client_data;

  bmfont_get_info(tcx->font, &font_width, &font_height);

  string    = paragraph;
  stringlen = (int) strlen(paragraph);

  pos.x = content->x0 + 4;
  pos.y = content->y0 + 4;

  while (stringlen > 0 && pos.y + font_height <= content->y1)
  {
    int            absolute_break, friendly_break;
    bmfont_width_t width;

    bmfont_measure(tcx->font, string, stringlen, content->x1 - 4 - pos.x, &absolute_break, &width);

    friendly_break = absolute_break;
    if (absolute_break < stringlen)
    {
      /* line didn't fit whole: try to break at the last space within it */
      for (friendly_break = absolute_break - 1; friendly_break > 0; friendly_break--)
        if (isspace((unsigned char) string[friendly_break]))
          break;
      if (friendly_break == 0)
        friendly_break = absolute_break; /* no space to break at: hard break */
    }

    bmfont_draw(tcx->font, scr, string, friendly_break, tcx->fg, tcx->bg, &pos, NULL);

    string    += friendly_break;
    stringlen -= friendly_break;
    while (stringlen > 0 && isspace((unsigned char) *string))
    {
      string++;
      stringlen--;
    }

    pos.x  = content->x0 + 4;
    pos.y += font_height + 2;
  }

  return result_OK;
}

#endif /* USE_SDL */
