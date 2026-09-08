/* framebuf/screen/screen-copy-bitmap-rle.c -- RLE bitmap blit */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "base/result.h"

#include "geom/box.h"

#include "framebuf/pixelfmt.h"

#include "framebuf/bitmap.h"
#include "framebuf/screen.h"

#include "../bitmap/rle.h"

#include "screen-copy-bitmap-rle.h"

result_t screen_copy_bitmap_rle(screen_t       *scr,
                                int             x,
                                int             y,
                                const bitmap_t *src,
                                const box_t    *draw_box)
{
  pixelfmt_t     base;
  int            log2bpp;
  const uint8_t *blob;
  const uint8_t *p;
  const uint8_t *end;
  uint8_t       *dstbase;
  int            firstrow;
  int            skip, plot;
  int            rows;
  int            bpp;
  int            i;

  assert(scr);
  assert(src);
  assert(pixelfmt_is_rle(src->format));

  base = pixelfmt_base(src->format);

  /* v1: 32bpp screen, source decodes to the screen's exact byte layout. */
  if (pixelfmt_log2bpp(scr->format) != 5)
    return result_NOT_SUPPORTED;
  if (base != scr->format)
    return result_NOT_SUPPORTED;

  log2bpp = pixelfmt_log2bpp(base);
  bpp     = 1 << (log2bpp - 3);

  blob = src->base;
  p    = blob + bitmap__RLE_HEADER_SIZE;
  end  = p + bitmap__rle_get32(blob + 4);

  /* Walk past rows clipped off the top. */
  firstrow = draw_box->y0 - y;
  for (i = 0; i < firstrow; i++)
    p = bitmap__rle_skip_row(p, end, log2bpp);

  skip = draw_box->x0 - x;
  plot = draw_box->x1 - draw_box->x0;
  rows = draw_box->y1 - draw_box->y0;

  dstbase = scr->base;
  for (i = 0; i < rows; i++)
  {
    uint8_t *dstrow;

    dstrow = dstbase
           + (size_t) (draw_box->y0 + i) * scr->rowbytes
           + (size_t) draw_box->x0 * bpp;

    p = bitmap__rle_decode_row(p, log2bpp, dstrow, skip, plot, 0);
  }

  return result_OK;
}
