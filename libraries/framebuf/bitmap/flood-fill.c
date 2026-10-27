/* framebuf/bitmap/flood-fill.c -- scanline flood fill */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "geom/box.h"
#include "geom/point.h"
#include "utils/array.h"

#include "framebuf/bitmap.h"

/* ----------------------------------------------------------------------- */

static pixelfmt_any32_t *flood_row(const bitmap_t *bm, int y)
{
  return (pixelfmt_any32_t *) ((uint8_t *) bm->base +
                               (size_t) y * bm->rowbytes);
}

result_t bitmap_flood_fill(bitmap_t *bm,
                           point_t   seed,
                           colour_t  colour,
                           box_t    *dirty)
{
  result_t          rc;
  box_t             extent = BOX_INIT;
  int               w, h;
  pixelfmt_any32_t  target, fillpx;
  point_t          *stack;
  int               n, allocated;
  pixelfmt_any32_t *row;
  int               x, x0, x1, y, dy, in_span;

  assert(bm);

  if (pixelfmt_is_rle(bm->format) || pixelfmt_log2bpp(bm->format) != 5)
    return result_NOT_SUPPORTED;

  rc = result_OK;
  w  = bm->size.w;
  h  = bm->size.h;

  if (seed.x < 0 || seed.x >= w || seed.y < 0 || seed.y >= h)
    goto exit;

  target = flood_row(bm, seed.y)[seed.x];
  fillpx = colour_to_pixel(NULL, 0, colour, bm->format);
  if (target == fillpx)
    goto exit;

  stack     = NULL;
  n         = 0;
  allocated = 0;
  if (array_grow((void **) &stack, sizeof(*stack), n, &allocated, 1, 256))
  {
    rc = result_OOM;
    goto exit;
  }

  stack[n++] = seed;

  while (n > 0)
  {
    n--;
    x   = stack[n].x;
    y   = stack[n].y;
    row = flood_row(bm, y);

    /* a seed pushed earlier may since have been filled over by another span */
    if (row[x] != target)
      continue;

    x0 = x;
    while (x0 > 0 && row[x0 - 1] == target)
      x0--;
    x1 = x;
    while (x1 < w - 1 && row[x1 + 1] == target)
      x1++;

    for (x = x0; x <= x1; x++)
      row[x] = fillpx;

    extent.x0 = MIN(extent.x0, x0);
    extent.x1 = MAX(extent.x1, x1 + 1);
    extent.y0 = MIN(extent.y0, y);
    extent.y1 = MAX(extent.y1, y + 1);

    /* one seed per run of target pixels in each adjacent row */
    for (dy = -1; dy <= 1; dy += 2)
    {
      if (y + dy < 0 || y + dy >= h)
        continue;

      row     = flood_row(bm, y + dy);
      in_span = 0;

      for (x = x0; x <= x1; x++)
      {
        if (row[x] != target)
        {
          in_span = 0;
          continue;
        }
        if (in_span)
          continue;

        in_span = 1;

        if (array_grow((void **) &stack, sizeof(*stack), n, &allocated, 1,
                       256))
        {
          rc = result_OOM;
          goto done;
        }

        stack[n++] = POINT(x, y + dy);
      }
    }
  }

done:
  free(stack);

exit:
  if (dirty)
    *dirty = extent;

  return rc;
}
