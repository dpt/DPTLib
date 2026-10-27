/* framebuf/bitmap/restore-outside-region.c -- copy back pixels outside a region */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/utils.h"
#include "geom/box.h"
#include "geom/region.h"

#include "framebuf/bitmap.h"

/* ----------------------------------------------------------------------- */

/* copy pixels [x0,x1) of row y from src to dst */
static void restore_span(bitmap_t       *dst,
                         const bitmap_t *src,
                         int             y,
                         int             x0,
                         int             x1,
                         int             bytespp)
{
  memcpy((uint8_t *) dst->base + (size_t) y * dst->rowbytes + x0 * bytespp,
         (const uint8_t *) src->base + (size_t) y * src->rowbytes +
         x0 * bytespp,
         (size_t) (x1 - x0) * bytespp);
}

result_t bitmap_restore_outside_region(bitmap_t       *dst,
                                       const bitmap_t *src,
                                       const region_t *region,
                                       const box_t    *box)
{
  int        log2bpp;
  int        bytespp;
  box_t      full;
  box_t      r;
  int        y;
  const int *runs;
  int        nruns;
  int        x, i;

  assert(dst);
  assert(src);
  assert(region);

  if (bitmap_is_compressed(src) || bitmap_is_compressed(dst))
    return result_NOT_SUPPORTED;

  if (dst->size.w != src->size.w ||
      dst->size.h != src->size.h ||
      dst->format != src->format)
    return result_INCOMPATIBLE;

  log2bpp = pixelfmt_log2bpp(dst->format);
  if (log2bpp < 3)
    return result_NOT_SUPPORTED;

  bytespp = 1 << (log2bpp - 3);

  full.x0 = 0;
  full.y0 = 0;
  full.x1 = dst->size.w;
  full.y1 = dst->size.h;

  if (box == NULL)
    r = full;
  else if (box_intersection(box, &full, &r))
    return result_OK; /* nothing to restore */

  for (y = r.y0; y < r.y1; y++)
  {
    nruns = region_row(region, y, &runs);

    /* restore the gaps between the region's runs */
    x = r.x0;
    for (i = 0; i < nruns && x < r.x1; i++)
    {
      if (runs[i * 2] > x)
        restore_span(dst, src, y, x, MIN(runs[i * 2], r.x1), bytespp);

      x = MAX(x, runs[i * 2 + 1]);
    }

    if (x < r.x1)
      restore_span(dst, src, y, x, r.x1, bytespp);
  }

  return result_OK;
}
