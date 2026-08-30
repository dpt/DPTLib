/* screen-fill-pattern.c -- fill a box with a repeating 8x8 two-colour pattern */

#include <assert.h>
#include <string.h>

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "geom/box.h"

#include "framebuf/screen.h"

/* One byte per row, MSB = leftmost pixel. */
static const unsigned char patterns[screen_PATTERN__LIMIT][8] =
{
  { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, /* SOLID      */
  { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 }, /* GREY50     */
  { 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00 }, /* HSTRIPE    */
  { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA }, /* VSTRIPE    */
  { 0x88, 0x44, 0x22, 0x11, 0x88, 0x44, 0x22, 0x11 }, /* DIAGONAL   */
  { 0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00 }, /* DOTS       */
  { 0xFF, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80 }, /* GRID       */
  { 0x80, 0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41 }  /* CROSSHATCH */
};

void screen_fill_pattern(screen_t        *scr,
                         const box_t     *box,
                         screen_pattern_t pattern,
                         int              origin_x,
                         int              origin_y,
                         colour_t         fg,
                         colour_t         bg)
{
  const unsigned char *tile;
  box_t                clip_box;
  box_t                draw_box;
  pixelfmt_any_t       fg_fmt, bg_fmt;
  pixelfmt_any_t       runs[8][8]; /* one expanded colour run per tile row */
  int                  xphase;
  int                  row, col, x, y;

  assert(pattern >= 0 && pattern < screen_PATTERN__LIMIT);
  if (pattern < 0 || pattern >= screen_PATTERN__LIMIT)
    return;

  tile = patterns[pattern];

  if (screen_get_clip(scr, &clip_box))
    return; /* invalid clipped screen */

  if (box_intersection(&clip_box, box, &draw_box))
    return; /* nothing visible */

  fg_fmt = colour_to_pixel(scr->palette,
                           (scr->format == pixelfmt_p4) ? 16 : 0,
                           fg, scr->format);
  bg_fmt = colour_to_pixel(scr->palette,
                           (scr->format == pixelfmt_p4) ? 16 : 0,
                           bg, scr->format);

  /* The tile is 8x8 and repeats, so there are only eight distinct pixel rows.
   * Expand each to a colour run once here, already phase-shifted for x, then
   * the scanline loops just index runs[row][col] with no per-pixel bit test. */
  xphase = ((draw_box.x0 - origin_x) & 7);
  for (row = 0; row < 8; row++)
  {
    unsigned char bits = tile[row];

    for (col = 0; col < 8; col++)
      runs[row][col] =
        (bits & (0x80u >> ((xphase + col) & 7))) ? fg_fmt : bg_fmt;
  }

  switch (pixelfmt_log2bpp(scr->format))
  {
  case 2:
    {
      unsigned char *rowp;

      rowp = (unsigned char *) scr->base + draw_box.y0 * scr->rowbytes;
      for (y = draw_box.y0; y < draw_box.y1; y++)
      {
        const pixelfmt_any_t *run = runs[(y - origin_y) & 7];

        col = 0;
        for (x = draw_box.x0; x < draw_box.x1; x++)
        {
          unsigned char *scrp  = rowp + (x >> 1);
          int            shift = (x & 1) * 4;

          *scrp = (unsigned char) ((*scrp & ~(0xF << shift)) |
                                   ((run[col] & 0xF) << shift));
          col = (col + 1) & 7;
        }
        rowp += scr->rowbytes;
      }
    }
    break;

  case 5:
    {
      unsigned char *rowp;

      rowp = (unsigned char *) scr->base + draw_box.y0 * scr->rowbytes;
      for (y = draw_box.y0; y < draw_box.y1; y++)
      {
        const pixelfmt_any_t *run = runs[(y - origin_y) & 7];
        pixelfmt_any32_t     *scrp = (pixelfmt_any32_t *) rowp + draw_box.x0;
        int                   w    = draw_box.x1 - draw_box.x0;

        /* leading partial tile up to an 8-pixel boundary, then whole runs */
        col = 0;
        while (w > 0)
        {
          int n = 8 - col;
          if (n > w)
            n = w;
          memcpy(scrp, run + col, (size_t) n * sizeof(*scrp));
          scrp += n;
          w    -= n;
          col   = 0;
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
