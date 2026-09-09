/* framebuf/screen/screen-copy-bitmap-rle.c -- RLE bitmap blit */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "base/result.h"

#include "geom/box.h"

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/pixelmap.h"

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

/* Blit a 32bpp RLE source onto a 4bpp paletted screen. The row codec only
 * emits whole 4-byte pixels, so each visible row is decoded (skip runs
 * zero-filled) into a scratch buffer, then converted pixel-by-pixel to the
 * nearest palette index -- matching screen_copy_bitmap_p4's alpha-tested
 * transfer. Zero-alpha pixels (skip runs, and genuinely transparent source
 * pixels) leave the background alone.
 *
 * ponytail: an opaque source with a 0x00000000 pixel (only possible for the
 * no-alpha *x8888 bases) loses that pixel, same trade-off as the raw p4
 * blit. Carry an explicit mask if opaque-black art shows up. */
static result_t screen_copy_bitmap_rle_p4(screen_t       *scr,
                                          int             x,
                                          int             y,
                                          const bitmap_t *src,
                                          const box_t    *draw_box)
{
  pixelfmt_t           base;
  int                  log2bpp;
  const uint8_t       *blob;
  const uint8_t       *p;
  const uint8_t       *end;
  uint8_t             *dstbase;
  pixelfmt_rgba8888_t *scratch;
  const pixelmap_t    *pm;
  int                  firstrow;
  int                  skip, plot;
  int                  rows;
  int                  i;

  base    = pixelfmt_base(src->format);
  log2bpp = pixelfmt_log2bpp(base);

  if (log2bpp != 5)
    return result_NOT_SUPPORTED; /* v1: 32bpp source only */

  blob = src->base;
  p    = blob + bitmap__RLE_HEADER_SIZE;
  end  = p + bitmap__rle_get32(blob + 4);

  firstrow = draw_box->y0 - y;
  for (i = 0; i < firstrow; i++)
    p = bitmap__rle_skip_row(p, end, log2bpp);

  skip = draw_box->x0 - x;
  plot = draw_box->x1 - draw_box->x0;
  rows = draw_box->y1 - draw_box->y0;

  /* The decode buffer is RGBA8888 byte order. The cached RGB->index table
   * turns the per-pixel nearest-palette match into a mask-and-lookup. */
  pm = pixelmap_get(pixelfmt_rgba8888, scr->format, scr->palette, 16);
  if (pm == NULL)
    return result_NOT_SUPPORTED;

  scratch = malloc((size_t) plot * sizeof(*scratch));
  if (scratch == NULL)
    return result_OOM;

  dstbase = scr->base;
  for (i = 0; i < rows; i++)
  {
    uint8_t *rowp;
    int      xx;

    for (xx = 0; xx < plot; xx++)
      scratch[xx] = 0; /* zero_skip below only fills covered skip runs */

    p = bitmap__rle_decode_row(p, log2bpp, scratch, skip, plot, 1);

    rowp = dstbase + (size_t) (draw_box->y0 + i) * scr->rowbytes;

    for (xx = 0; xx < plot; xx++)
    {
      colour_t       c;
      unsigned int   r, g, b, idx;
      int            dstx;
      uint8_t       *scrp;
      int            shift;
      pixelfmt_any_t pxl;

      c.primary = scratch[xx];
      if (colour_get_alpha(&c) == 0)
        continue;

      dstx  = draw_box->x0 + xx;
      scrp  = rowp + (dstx >> 1);
      shift = (dstx & 1) * 4;

      r   = (c.primary >> pm->rshift) & 0xFF;
      g   = (c.primary >> pm->gshift) & 0xFF;
      b   = (c.primary >> pm->bshift) & 0xFF;
      idx = ((r >> (8 - pm->rbits)) << (pm->gbits + pm->bbits))
          | ((g >> (8 - pm->gbits)) << pm->bbits)
          | ( b >> (8 - pm->bbits));
      pxl = (pm->entries[idx >> 1] >> ((idx & 1) << 2)) & 0xF;

      *scrp = (uint8_t) ((*scrp & ~(0xF << shift)) | ((pxl & 0xF) << shift));
    }
  }

  free(scratch);

  return result_OK;
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

  if (pixelfmt_log2bpp(scr->format) == 2)
    return screen_copy_bitmap_rle_p4(scr, x, y, src, draw_box);

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
