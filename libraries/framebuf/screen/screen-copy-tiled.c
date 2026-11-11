/* framebuf/screen/screen-copy-tiled.c -- tiled bitmap drawing */

#include <stdlib.h>
#include <string.h>

#include "base/utils.h"
#include "geom/box.h"
#include "geom/point.h"

#include "framebuf/bitmap.h"
#include "framebuf/screen.h"

/* ----------------------------------------------------------------------- */

/* Left (or top) edge of the tile containing "v", for tiles "step" wide that
 * have one edge at "origin". Correct for v below origin, where % goes
 * negative. */
static int tile_start(int v, int origin, int step)
{
  int d;

  d = (v - origin) % step;
  if (d < 0)
    d += step;

  return v - d;
}

/* Pre-tiling: a tiny tile costs one blit call per copy, so it is first
 * repeated into a block at least this many pixels along each axis. */
#define PRETILE_MIN 64

/* The repeat count along one axis for a tile "n" pixels long. A sub-tile
 * dithers phased to the block, not to itself, so an axis whose length is not
 * a multiple of the dither "period" is left unrepeated. */
static int pretile_count(int n, int period)
{
  if (n >= PRETILE_MIN || n % period != 0)
    return 1;

  return DIVIDE_ROUNDING_UP(PRETILE_MIN, n);
}

/* Make "block" from "src" repeated kx by ky times, sharing src's palette and
 * span. Returns non-zero on success; the caller frees block->base. */
static int pretile(const bitmap_t *src, int kx, int ky, bitmap_t *block)
{
  int                  rowlen;
  unsigned char       *base;
  int                  y;
  unsigned char       *row;
  const unsigned char *srow;
  int                  i;

  rowlen = src->size.w << (pixelfmt_log2bpp(src->format) - 3);
  base   = malloc((size_t) rowlen * kx * src->size.h * ky);
  if (base == NULL)
    return 0;

  *block          = *src;
  block->size     = SIZE2D(src->size.w * kx, src->size.h * ky);
  block->rowbytes = rowlen * kx;
  block->base     = base;

  for (y = 0; y < block->size.h; y++)
  {
    row = base + (size_t) y * block->rowbytes;
    if (y >= src->size.h)
    {
      memcpy(row, row - (size_t) src->size.h * block->rowbytes,
             block->rowbytes);
      continue;
    }

    srow = (const unsigned char *) src->base + (size_t) y * src->rowbytes;
    for (i = 0; i < kx; i++)
      memcpy(row + i * rowlen, srow, rowlen);
  }

  return 1;
}

/* ----------------------------------------------------------------------- */

result_t screen_copy_bitmap_tiled(screen_t       *scr,
                                  const box_t    *dst,
                                  const bitmap_t *src,
                                  point_t         origin)
{
  return screen_copy_bitmap_tiled_dithered(scr, dst, src, origin,
                                           screen_DITHER_NEAREST);
}

result_t screen_copy_bitmap_tiled_dithered(screen_t       *scr,
                                           const box_t    *dst,
                                           const bitmap_t *src,
                                           point_t         origin,
                                           screen_dither_t method)
{
  result_t rc;
  box_t    orig_clip;
  box_t    clip;
  int      period;
  int      kx;
  int      ky;
  bitmap_t block;
  int      pretiled;
  int      x0;
  int      x;
  int      y;

  if (src->size.w <= 0 || src->size.h <= 0)
    return result_BAD_ARG;

  if (box_is_empty(dst))
    return result_OK;

  /* An empty screen clip means "no clipping", so "dst" alone bounds the
   * drawing. */
  orig_clip = scr->clip;
  if (box_is_empty(&orig_clip))
    clip = *dst;
  else if (box_intersection(&orig_clip, dst, &clip))
    return result_OK; /* dst entirely outside the clip */

  if (box_is_empty(&clip))
    return result_OK;

  scr->clip = clip;

  /* Pre-tile a tiny raw source with whole bytes per pixel. The dither is
   * ordered, so a block reproduces per-tile phasing only along an axis that
   * is a multiple of its period; a 32bpp screen does not dither. */
  pretiled = 0;
  if (!bitmap_is_compressed(src) && pixelfmt_log2bpp(src->format) >= 3)
  {
    if (method == screen_DITHER_NEAREST || pixelfmt_log2bpp(scr->format) >= 5)
      period = 1;
    else if (method == screen_DITHER_BAYER)
      period = 8;
    else
      period = 64;

    kx = pretile_count(src->size.w, period);
    ky = pretile_count(src->size.h, period);
    if (kx * ky > 1 && pretile(src, kx, ky, &block))
    {
      src      = &block;
      pretiled = 1;
    }
  }

  rc = result_OK;
  x0 = tile_start(clip.x0, origin.x, src->size.w);
  y  = tile_start(clip.y0, origin.y, src->size.h);
  for (; y < clip.y1 && rc == result_OK; y += src->size.h)
    for (x = x0; x < clip.x1 && rc == result_OK; x += src->size.w)
      rc = screen_copy_bitmap_dithered(scr, x, y, src, method);

  if (pretiled)
    free(block.base);

  scr->clip = orig_clip;

  return rc;
}
