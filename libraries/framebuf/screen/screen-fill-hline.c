/* framebuf/screen/screen-fill-hline.c -- horizontal run fill */

#include <assert.h>
#include <stddef.h>

#include "framebuf/colour.h"

#include "framebuf/screen.h"

/* ----------------------------------------------------------------------- */

void screen_fill_hline(screen_t *scr, int x, int y, int w, colour_t colour)
{
  box_t          clip_box;
  box_t          run_box;
  box_t          draw_box;
  int            clipped_width;
  pixelfmt_any_t fmt;

  if (w <= 0)
    return;

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  run_box.x0 = x;
  run_box.y0 = y;
  run_box.x1 = x + w;
  run_box.y1 = y + 1;
  if (box_intersection(&clip_box, &run_box, &draw_box))
    return;

  clipped_width = draw_box.x1 - draw_box.x0;

  fmt = colour_to_pixel(scr->palette,
                        (scr->format == pixelfmt_p4) ? 16 : 0,
                        colour, scr->format);
  switch (pixelfmt_log2bpp(scr->format))
  {
  case 2:
    {
      unsigned char *rowp;
      int            xx;

      rowp = (unsigned char *) scr->base + draw_box.y0 * scr->rowbytes;
      for (xx = 0; xx < clipped_width; xx++)
      {
        int            px;
        unsigned char *scrp;
        int            shift;

        px    = draw_box.x0 + xx;
        scrp  = rowp + (px >> 1);
        shift = (px & 1) * 4;

        *scrp = (unsigned char) ((*scrp & ~(0xF << shift)) | ((fmt & 0xF) << shift));
      }
    }
    break;

  case 5:
    {
      pixelfmt_any32_t *scrp;
      int               ww;

      scrp = scr->base;
      scrp += draw_box.y0 * scr->rowbytes / sizeof(*scrp) + draw_box.x0;
      for (ww = clipped_width; ww > 0; ww--)
        *scrp++ = fmt;
    }
    break;

  default:
    assert(!"Unimplemented pixel format");
    break;
  }
}
