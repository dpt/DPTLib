/* geom/region/translate.c -- a region shifted and clipped */

#include "base/utils.h"

#include "impl.h"

result_t region_translate(const region_t *region,
                          int             dx,
                          int             dy,
                          const box_t    *clip,
                          region_t      **out)
{
  region__builder_t b;
  result_t          rc;
  box_t             bounds;
  box_t             moved;
  box_t             within;
  const int        *runs;
  int               y, i, n;

  *out = NULL;

  if (region == NULL)
    return result_OK;

  bounds = region_bounds(region);
  box_translated(&bounds, dx, dy, &moved);
  if (box_intersection(&moved, clip, &within))
    return result_OK;

  rc = region__builder_init(&b, within.y0, within.y1 - within.y0);
  if (rc != result_OK)
    return rc;

  for (y = within.y0; rc == result_OK && y < within.y1; y++)
  {
    region__builder_row(&b, y);

    n = region_row(region, y - dy, &runs);
    for (i = 0; rc == result_OK && i < n; i++)
      rc = region__builder_add(&b,
                               MAX(runs[i * 2] + dx, within.x0),
                               MIN(runs[i * 2 + 1] + dx, within.x1));
  }

  if (rc != result_OK)
  {
    region__builder_abort(&b);
    return rc;
  }

  return region__builder_finish(&b, out);
}
