/* framebuf/screen/screen-fill-pattern.c -- fill a box with a repeating 8x8 pattern */

#include <assert.h>
#include <string.h>

#include "framebuf/colour.h"
#include "framebuf/pattern.h"
#include "framebuf/pixelfmt.h"
#include "geom/box.h"

#include "framebuf/screen.h"

void screen_fill_pattern(screen_t        *scr,
                         const box_t     *box,
                         const pattern_t *pattern)
{
  box_t          clip_box;
  box_t          draw_box;
  int            stencil;
  pixelfmt_any_t fg_fmt, bg_fmt;
  pixelfmt_any_t runs[8][8]; /* one expanded colour run per tile row */
  int            xphase;
  int            row, col, x, y;

  assert(scr);
  assert(pattern);

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  if (box_intersection(&clip_box, box, &draw_box))
    return; /* nothing visible */

  stencil = (pattern->flags & pattern_FLAG_STENCIL) != 0;

  fg_fmt = colour_to_pixel(scr->palette,
                           (scr->format == pixelfmt_p4) ? 16 : 0,
                           pattern->fg, scr->format);
  bg_fmt = colour_to_pixel(scr->palette,
                           (scr->format == pixelfmt_p4) ? 16 : 0,
                           pattern->bg, scr->format);

  /* The tile is 8x8 and repeats, so there are only eight distinct pixel rows.
   * Expand each to a colour run once here, already phase-shifted for x, then
   * the scanline loops just index runs[row][col]. Stencil rows re-test the
   * pattern bit per pixel rather than using the run. */
  xphase = ((draw_box.x0 - pattern->origin.x) & 7);
  for (row = 0; row < 8; row++)
  {
    uint8_t bits;

    bits = pattern->bits[row];
    for (col = 0; col < 8; col++)
      runs[row][col] =
        (bits & (0x80u >> ((xphase + col) & 7))) ? fg_fmt : bg_fmt;
  }

  switch (pixelfmt_log2bpp(scr->format))
  {
  case 2:
    {
      unsigned char        *rowp;
      const pixelfmt_any_t *run;
      uint8_t               bits;
      unsigned char        *scrp;
      int                   shift;

      rowp = (unsigned char *) scr->base + draw_box.y0 * scr->rowbytes;
      for (y = draw_box.y0; y < draw_box.y1; y++)
      {
        row  = (y - pattern->origin.y) & 7;
        run  = runs[row];
        bits = pattern->bits[row];
        col  = 0;
        for (x = draw_box.x0; x < draw_box.x1; x++)
        {
          if (!stencil ||
              (bits & (0x80u >> ((x - pattern->origin.x) & 7))))
          {
            scrp  = rowp + (x >> 1);
            shift = (x & 1) * 4;

            *scrp = (unsigned char) ((*scrp & ~(0xF << shift)) |
                                     ((run[col] & 0xF) << shift));
          }
          col = (col + 1) & 7;
        }
        rowp += scr->rowbytes;
      }
    }
    break;

  case 5:
    {
      unsigned char        *rowp;
      const pixelfmt_any_t *run;
      pixelfmt_any32_t     *scrp;
      uint8_t               bits;
      int                   w;
      int                   n;

      rowp = (unsigned char *) scr->base + draw_box.y0 * scr->rowbytes;
      for (y = draw_box.y0; y < draw_box.y1; y++)
      {
        row  = (y - pattern->origin.y) & 7;
        run  = runs[row];
        bits = pattern->bits[row];
        scrp = (pixelfmt_any32_t *) rowp + draw_box.x0;
        w    = draw_box.x1 - draw_box.x0;

        if (!stencil)
        {
          /* leading partial tile up to an 8-pixel boundary, then whole runs */
          col = 0;
          while (w > 0)
          {
            n = 8 - col;
            if (n > w)
              n = w;
            memcpy(scrp, run + col, (size_t) n * sizeof(*scrp));
            scrp += n;
            w    -= n;
            col   = 0;
          }
        }
        else
        {
          for (x = draw_box.x0; x < draw_box.x1; x++)
          {
            if (bits & (0x80u >> ((x - pattern->origin.x) & 7)))
              *scrp = fg_fmt;
            scrp++;
          }
        }
        rowp += scr->rowbytes;
      }
    }
    break;

  default:
    assert(!"Unimplemented pixel format");
    break;
  }
}
