/* geom/region/flood.c -- region of the pixels connected to a seed */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "utils/array.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

/* ponytail: one byte per pixel of `within` marks what the fill has taken,
 * so scratch is O(area) for the duration of the call. Track per-row spans
 * instead if huge areas ever matter. */
#define SEEN(x, y) seen[(size_t) ((y) - within->y0) * (size_t) w + \
                        (size_t) ((x) - within->x0)]

/* an untaken pixel the caller wants */
#define OPEN(x, y) (!SEEN(x, y) && inside(opaque, x, y))

result_t region_from_flood(point_t             seed,
                           const box_t        *within,
                           region_inside_fn_t *inside,
                           void               *opaque,
                           region_t          **out)
{
  result_t          rc;
  int               w, h;
  unsigned char    *seen;
  point_t          *stack;
  int               n, allocated;
  box_t             extent = BOX_INIT;
  int               x, x0, x1, y, dy, in_span;
  region__builder_t b;

  *out = NULL;

  if (seed.x < within->x0 || seed.x >= within->x1 ||
      seed.y < within->y0 || seed.y >= within->y1 ||
      !inside(opaque, seed.x, seed.y))
    return result_OK;

  w    = within->x1 - within->x0;
  h    = within->y1 - within->y0;
  seen = calloc((size_t) w * (size_t) h, 1);
  if (seen == NULL)
    return result_OOM;

  rc        = result_OK;
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
    x = stack[n].x;
    y = stack[n].y;

    /* a seed pushed earlier may since have been taken by another span */
    if (SEEN(x, y))
      continue;

    x0 = x;
    while (x0 > within->x0 && OPEN(x0 - 1, y))
      x0--;
    x1 = x;
    while (x1 < within->x1 - 1 && OPEN(x1 + 1, y))
      x1++;

    for (x = x0; x <= x1; x++)
      SEEN(x, y) = 1;

    extent.x0 = MIN(extent.x0, x0);
    extent.x1 = MAX(extent.x1, x1 + 1);
    extent.y0 = MIN(extent.y0, y);
    extent.y1 = MAX(extent.y1, y + 1);

    /* one seed per run of open pixels in each adjacent row */
    for (dy = -1; dy <= 1; dy += 2)
    {
      if (y + dy < within->y0 || y + dy >= within->y1)
        continue;

      in_span = 0;

      for (x = x0; x <= x1; x++)
      {
        if (!OPEN(x, y + dy))
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
          goto exit;
        }

        stack[n++] = POINT(x, y + dy);
      }
    }
  }

  rc = region__builder_init(&b, extent.y0, extent.y1 - extent.y0);
  if (rc != result_OK)
    goto exit;

  for (y = extent.y0; rc == result_OK && y < extent.y1; y++)
  {
    region__builder_row(&b, y);

    for (x = extent.x0; rc == result_OK && x < extent.x1; x++)
    {
      if (!SEEN(x, y))
        continue;

      x0 = x;
      while (x < extent.x1 && SEEN(x, y))
        x++;
      rc = region__builder_add(&b, x0, x);
    }
  }

  if (rc == result_OK)
    rc = region__builder_finish(&b, out);
  else
    region__builder_abort(&b);

exit:
  free(stack);
  free(seen);

  return rc;
}
