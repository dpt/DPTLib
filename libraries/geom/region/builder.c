/* geom/region/builder.c -- incremental construction of regions */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "base/utils.h"
#include "utils/array.h"

#include "impl.h"

result_t region__builder_init(region__builder_t *b, int y0, int h)
{
  b->runs     = NULL;
  b->n        = 0;
  b->cap      = 0;
  b->y0       = y0;
  b->h        = h;
  b->rowstart = 0;
  b->start    = malloc((size_t) (h + 1) * sizeof(*b->start));

  return b->start == NULL ? result_OOM : result_OK;
}

void region__builder_row(region__builder_t *b, int y)
{
  b->rowstart        = b->n;
  b->start[y - b->y0] = b->n;
}

result_t region__builder_add(region__builder_t *b, int x0, int x1)
{
  if (x0 >= x1)
    return result_OK;

  if (b->n > b->rowstart && x0 <= b->runs[b->n - 1])
  {
    b->runs[b->n - 1] = MAX(b->runs[b->n - 1], x1);
    return result_OK;
  }

  if (array_grow((void **) &b->runs, sizeof(*b->runs), b->n, &b->cap, 2, 64))
    return result_OOM;

  b->runs[b->n++] = x0;
  b->runs[b->n++] = x1;
  return result_OK;
}

void region__builder_abort(region__builder_t *b)
{
  free(b->start);
  free(b->runs);
  b->start = NULL;
  b->runs  = NULL;
}

result_t region__builder_finish(region__builder_t *b, region_t **out)
{
  region_t *r;
  int       i, first, last, xmin, xmax;

  *out  = NULL;
  first = -1;
  last  = -1;
  xmin  = INT_MAX;
  xmax  = INT_MIN;

  b->start[b->h] = b->n;

  for (i = 0; i < b->h; i++)
  {
    if (b->start[i + 1] == b->start[i])
      continue;

    if (first < 0)
      first = i;
    last = i;
    xmin = MIN(xmin, b->runs[b->start[i]]);
    xmax = MAX(xmax, b->runs[b->start[i + 1] - 1]);
  }

  if (first < 0)
  {
    region__builder_abort(b);
    return result_OK;
  }

  r = malloc(sizeof(*r));
  if (r == NULL)
  {
    region__builder_abort(b);
    return result_OOM;
  }

  /* drop empty leading rows; the end sentinel follows the last used row */
  if (first > 0)
    memmove(b->start, b->start + first,
            (size_t) (last - first + 1) * sizeof(*b->start));
  b->start[last - first + 1] = b->n;

  r->start     = b->start;
  r->runs      = b->runs;
  r->bounds.x0 = xmin;
  r->bounds.x1 = xmax;
  r->bounds.y0 = b->y0 + first;
  r->bounds.y1 = b->y0 + last + 1;

  *out = r;
  return result_OK;
}
