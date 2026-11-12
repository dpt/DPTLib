/* framebuf/bitmap/content-bounds.c -- bound the pixels unlike a blank */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "base/utils.h"
#include "framebuf/pixelfmt.h"

#include "framebuf/bitmap.h"

/* ----------------------------------------------------------------------- */

/* pixel x of row, of 8, 16 or 32 bpp */
static unsigned int pixel_at(const uint8_t *row, int log2bpp, int x)
{
  switch (log2bpp)
  {
  case 3:
    return row[x];
  case 4:
    return ((const uint16_t *) row)[x];
  default:
    return ((const uint32_t *) row)[x];
  }
}

box_t bitmap_content_bounds(const bitmap_t *bm,
                            unsigned int    blank,
                            unsigned int    mask)
{
  box_t          box = BOX_INIT;
  int            log2bpp;
  int            y;
  const uint8_t *row;
  int            x0, x1;

  assert(bm);

  log2bpp = pixelfmt_log2bpp(bm->format);
  if (bitmap_is_compressed(bm) || log2bpp < 3 || log2bpp > 5)
    return (box_t) BOX_POS_SIZE(0, 0, bm->size.w, bm->size.h);

  blank &= mask;
  for (y = 0; y < bm->size.h; y++)
  {
    row = (const uint8_t *) bm->base + (size_t) y * bm->rowbytes;

    /* in from the left, then in from the right to the first found */
    for (x0 = 0; x0 < bm->size.w; x0++)
      if ((pixel_at(row, log2bpp, x0) & mask) != blank)
        break;
    if (x0 == bm->size.w)
      continue;
    for (x1 = bm->size.w; x1 - 1 > x0; x1--)
      if ((pixel_at(row, log2bpp, x1 - 1) & mask) != blank)
        break;

    box.x0 = MIN(box.x0, x0);
    box.x1 = MAX(box.x1, x1);
    box.y0 = MIN(box.y0, y);
    box.y1 = y + 1;
  }

  return box;
}
