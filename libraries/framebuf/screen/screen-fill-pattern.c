/* framebuf/screen/screen-fill-pattern.c -- fill a box with a repeating 8x8 pattern */

#include <assert.h>
#include <string.h>

#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/pattern.h"
#include "framebuf/pixelfmt.h"
#include "geom/box.h"

#include "framebuf/screen.h"

/* runs[row][col] is the pattern's expanded colour for tile row "row" at
 * screen column (draw_box.x0 + col), i.e. already phase-shifted for x. The
 * scanline helpers below just index it; stencil paths re-test the pattern
 * bit per pixel rather than using the run. */
typedef pixelfmt_any_t pattern_runs_t[8][8];

static void screen_fill_pattern_p4(screen_t            *scr,
                                   const pattern_t     *pattern,
                                   const box_t         *draw_box,
                                   int                  stencil,
                                   const pattern_runs_t runs)
{
  unsigned char *rowp;
  int            row, col, x, y;

  rowp = (unsigned char *) scr->base + draw_box->y0 * scr->rowbytes;
  for (y = draw_box->y0; y < draw_box->y1; y++)
  {
    const pixelfmt_any_t *run;
    uint8_t               bits;

    row  = (y - pattern->origin.y) & 7;
    run  = runs[row];
    bits = pattern->bits[row];
    col  = 0;
    for (x = draw_box->x0; x < draw_box->x1; x++)
    {
      if (!stencil ||
          (bits & (0x80u >> ((x - pattern->origin.x) & 7))))
      {
        unsigned char *scrp;
        int            shift;

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

static void screen_fill_pattern_32(screen_t            *scr,
                                   const pattern_t     *pattern,
                                   const box_t         *draw_box,
                                   int                  stencil,
                                   pixelfmt_any_t       fg_fmt,
                                   const pattern_runs_t runs)
{
  unsigned char *rowp;
  int            row, col, x, y;

  rowp = (unsigned char *) scr->base + draw_box->y0 * scr->rowbytes;
  for (y = draw_box->y0; y < draw_box->y1; y++)
  {
    const pixelfmt_any_t *run;
    uint8_t               bits;
    pixelfmt_any32_t     *scrp;
    int                   w;
    int                   n;

    row  = (y - pattern->origin.y) & 7;
    run  = runs[row];
    bits = pattern->bits[row];
    scrp = (pixelfmt_any32_t *) rowp + draw_box->x0;
    w    = draw_box->x1 - draw_box->x0;

    if (!stencil)
    {
      /* leading partial tile up to an 8-pixel boundary, then whole runs */
      col = 0;
      while (w > 0)
      {
        n = MIN(8 - col, w);
        memcpy(scrp, run + col, (size_t) n * sizeof(*scrp));
        scrp += n;
        w    -= n;
        col   = 0;
      }
    }
    else
    {
      for (x = draw_box->x0; x < draw_box->x1; x++)
      {
        if (bits & (0x80u >> ((x - pattern->origin.x) & 7)))
          *scrp = fg_fmt;
        scrp++;
      }
    }
    rowp += scr->rowbytes;
  }
}

void screen_fill_pattern(screen_t        *scr,
                         const box_t     *box,
                         const pattern_t *pattern)
{
  box_t          clip_box;
  box_t          draw_box;
  int            stencil;
  pixelfmt_any_t fg_fmt, bg_fmt;
  pattern_runs_t runs; /* one expanded colour run per tile row */
  int            xphase;
  int            row, col;

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
    screen_fill_pattern_p4(scr, pattern, &draw_box, stencil, runs);
    break;

  case 5:
    screen_fill_pattern_32(scr, pattern, &draw_box, stencil, fg_fmt, runs);
    break;

  default:
    assert(!"Unimplemented pixel format");
    break;
  }
}
