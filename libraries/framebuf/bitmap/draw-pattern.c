/* framebuf/bitmap/draw-pattern.c -- pattern-masked rectangle fill */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "geom/box.h"

#include "framebuf/bitmap.h"

result_t bitmap_draw_pattern(bitmap_t     *bm,
                             const box_t  *area,
                             colour_t      colour,
                             const uint8_t mask[8])
{
  int             log2bpp;
  pixelfmt_any_t  px;
  box_t           full;
  box_t           clip;
  int             x, y;

  assert(bm);
  assert(mask);

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

  px = colour_to_pixel(bm->palette,
                       bm->palette ? 1 << (1 << log2bpp) : 0,
                       colour,
                       bm->format);

  if (log2bpp == 3) /* 8bpp */
  {
    uint8_t *base;
    uint8_t *row;

    base = bm->base;
    for (y = clip.y0; y < clip.y1; y++)
    {
      uint8_t bits = mask[y & 7];

      row = base + (size_t) y * bm->rowbytes;
      for (x = clip.x0; x < clip.x1; x++)
        if (bits & (0x80 >> (x & 7)))
          row[x] = (uint8_t) px;
    }
  }
  else /* 32bpp */
  {
    uint8_t          *base;
    pixelfmt_any32_t *row;

    base = bm->base;
    for (y = clip.y0; y < clip.y1; y++)
    {
      uint8_t bits = mask[y & 7];

      row = (pixelfmt_any32_t *) (base + (size_t) y * bm->rowbytes);
      for (x = clip.x0; x < clip.x1; x++)
        if (bits & (0x80 >> (x & 7)))
          row[x] = px;
    }
  }

  return result_OK;
}
