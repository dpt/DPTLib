/* geom/region/region.c -- creation, queries and destruction of regions */

#include <stdlib.h>

#include "base/utils.h"

#include "geom/region.h"

#include "impl.h"

void region_destroy(region_t *region)
{
  if (region == NULL)
    return;

  free(region->start);
  free(region->runs);
  free(region);
}

result_t region_from_box(const box_t *box, region_t **out)
{
  region_t *r;
  int       h, i;

  *out = NULL;

  if (box_is_empty(box))
    return result_OK;

  h = box->y1 - box->y0;
  r = malloc(sizeof(*r));
  if (r == NULL)
    return result_OOM;

  r->bounds = *box;
  r->start  = malloc((size_t) (h + 1) * sizeof(*r->start));
  r->runs   = malloc((size_t) h * 2 * sizeof(*r->runs));
  if (r->start == NULL || r->runs == NULL)
  {
    region_destroy(r);
    return result_OOM;
  }

  for (i = 0; i < h; i++)
  {
    r->start[i]    = i * 2;
    r->runs[i * 2]     = box->x0;
    r->runs[i * 2 + 1] = box->x1;
  }
  r->start[h] = h * 2;

  *out = r;
  return result_OK;
}

box_t region_bounds(const region_t *region)
{
  box_t empty = BOX_INIT;

  return region == NULL ? empty : region->bounds;
}

int region_row(const region_t *region, int y, const int **runs)
{
  int r;

  if (region == NULL || y < region->bounds.y0 || y >= region->bounds.y1)
  {
    *runs = NULL;
    return 0;
  }

  r     = y - region->bounds.y0;
  *runs = region->runs + region->start[r];

  return (region->start[r + 1] - region->start[r]) / 2;
}

int region_contains(const region_t *region, int x, int y)
{
  const int *runs;
  int        lo, hi, mid, n;

  n = region_row(region, y, &runs);

  /* binary search for the last run starting at or before x */
  lo = 0;
  hi = n;
  while (lo < hi)
  {
    mid = (lo + hi) / 2;
    if (runs[mid * 2] <= x)
      lo = mid + 1;
    else
      hi = mid;
  }

  return lo > 0 && x < runs[(lo - 1) * 2 + 1];
}
