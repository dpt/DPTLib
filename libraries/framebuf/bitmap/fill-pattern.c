/* framebuf/bitmap/fill-pattern.c -- fill a bitmap rectangle with an 8x8 pattern */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "geom/box.h"

#include "framebuf/pattern.h"

#include "framebuf/bitmap.h"

result_t bitmap_fill_pattern(bitmap_t        *bm,
                             const box_t     *area,
                             const pattern_t *pattern)
{
  int            log2bpp;
  int            stencil;
  pixelfmt_any_t fg;
  pixelfmt_any_t bg;
  box_t          full;
  box_t          clip;
  int            xphase, yphase;
  int            x, y;

  assert(bm);
  assert(pattern);

  log2bpp = pixelfmt_log2bpp(bm->format);
  if (log2bpp != 3 && log2bpp != 5)
    return result_NOT_SUPPORTED;

  full.x0 = 0;
  full.y0 = 0;
  full.x1 = bm->size.w;
  full.y1 = bm->size.h;

  if (area == NULL)
  {
    clip = full;
  }
  else if (!box_intersection(area, &full, &clip))
  {
    return result_OK; /* nothing to fill */
  }

  stencil = (pattern->flags & pattern_FLAG_STENCIL) != 0;

  fg = colour_to_pixel(bm->palette,
                       bm->palette ? 1 << (1 << log2bpp) : 0,
                       pattern->fg,
                       bm->format);
  bg = colour_to_pixel(bm->palette,
                       bm->palette ? 1 << (1 << log2bpp) : 0,
                       pattern->bg,
                       bm->format);

  /* The tile phase: the coordinate in "origin" is the one that maps to the
   * pattern's leftmost/topmost bit. */
  xphase = pattern->origin.x;
  yphase = pattern->origin.y;

  if (log2bpp == 3) /* 8bpp */
  {
    uint8_t *base;
    uint8_t *row;

    base = bm->base;
    for (y = clip.y0; y < clip.y1; y++)
    {
      uint8_t bits = pattern->bits[(y - yphase) & 7];

      row = base + (size_t) y * bm->rowbytes;
      for (x = clip.x0; x < clip.x1; x++)
        if (bits & (0x80 >> ((x - xphase) & 7)))
          row[x] = (uint8_t) fg;
        else if (!stencil)
          row[x] = (uint8_t) bg;
    }
  }
  else /* 32bpp */
  {
    uint8_t          *base;
    pixelfmt_any32_t *row;

    base = bm->base;
    for (y = clip.y0; y < clip.y1; y++)
    {
      uint8_t bits = pattern->bits[(y - yphase) & 7];

      row = (pixelfmt_any32_t *) (base + (size_t) y * bm->rowbytes);
      for (x = clip.x0; x < clip.x1; x++)
        if (bits & (0x80 >> ((x - xphase) & 7)))
          row[x] = fg;
        else if (!stencil)
          row[x] = bg;
    }
  }

  return result_OK;
}
