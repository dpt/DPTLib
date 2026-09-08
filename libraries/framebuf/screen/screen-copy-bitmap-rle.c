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

/* Map a 32bpp 8888 format to its no-alpha ("x") sibling: bgra->bgrx,
 * rgba->rgbx, etc. The RLE blit writes whole 4-byte pixels and honours skip
 * runs for transparency, so an alpha source and its matching x screen (or the
 * reverse) decode identically -- only the channel order has to agree. Passes
 * non-8888 formats through unchanged. */
static pixelfmt_t rle__dealpha(pixelfmt_t f)
{
  switch (f)
  {
  case pixelfmt_bgra8888: return pixelfmt_bgrx8888;
  case pixelfmt_rgba8888: return pixelfmt_rgbx8888;
  case pixelfmt_abgr8888: return pixelfmt_xbgr8888;
  case pixelfmt_argb8888: return pixelfmt_xrgb8888;
  default:                return f;
  }
}

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

  /* v1: 32bpp screen, source decodes to the screen's channel order. An alpha
   * source onto its matching x screen (or vice versa) is fine -- the decode
   * is byte-identical -- so compare with alpha folded out. */
  if (pixelfmt_log2bpp(scr->format) != 5)
    return result_NOT_SUPPORTED;
  if (rle__dealpha(base) != rle__dealpha(scr->format))
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
