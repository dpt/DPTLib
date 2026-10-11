/* geom/region/invert.c -- complement of a region within a box */

#include "base/utils.h"

#include "impl.h"

result_t region_invert(const region_t *region,
                       const box_t    *within,
                       region_t      **out)
{
  region__builder_t b;
  result_t          rc;
  const int        *runs;
  int               y, i, n, x;

  *out = NULL;

  if (box_is_empty(within))
    return result_OK;

  rc = region__builder_init(&b, within->y0, within->y1 - within->y0);
  if (rc != result_OK)
    return rc;

  for (y = within->y0; rc == result_OK && y < within->y1; y++)
  {
    region__builder_row(&b, y);

    n = region_row(region, y, &runs);
    x = within->x0;
    for (i = 0; rc == result_OK && i < n && x < within->x1; i++)
    {
      rc = region__builder_add(&b, x, MIN(runs[i * 2], within->x1));
      x  = MAX(x, runs[i * 2 + 1]);
    }

    if (rc == result_OK)
      rc = region__builder_add(&b, x, within->x1);
  }

  if (rc != result_OK)
  {
    region__builder_abort(&b);
    return rc;
  }

  return region__builder_finish(&b, out);
}
