/* geom/region/polygon.c -- build a region from a polygon (even-odd fill) */

#include <stdlib.h>

#include "base/utils.h"

#include "geom/region.h"

#include "impl.h"

typedef struct
{
  point_t p, q; /* ends as given; p.y != q.y */
  int     ymin; /* first scanline crossed */
  int     ymax; /* one past the last */
}
edge_t;

static int edge_cmp(const void *a, const void *b)
{
  return ((const edge_t *) a)->ymin - ((const edge_t *) b)->ymin;
}

result_t region_from_polygon(const point_t *pts,
                             int            n,
                             const box_t   *clip,
                             region_t     **out)
{
  result_t          rc;
  region__builder_t b;
  edge_t           *edges;
  edge_t          **active;
  int              *xs;
  int               ne, na, next, nx, y, y0, y1, i, j, k, t;
  point_t           p, q;

  *out = NULL;

  if (n < 3 || box_is_empty(clip))
    return result_OK;

  edges  = malloc((size_t) n * sizeof(*edges));
  active = malloc((size_t) n * sizeof(*active));
  xs     = malloc((size_t) n * sizeof(*xs));
  rc     = (edges && active && xs) ? result_OK : result_OOM;

  /* horizontal edges never cross a scanline centre */
  ne = 0;
  for (i = 0; rc == result_OK && i < n; i++)
  {
    p = pts[i];
    q = pts[(i + 1) % n];
    if (p.y == q.y)
      continue;

    edges[ne].p    = p;
    edges[ne].q    = q;
    edges[ne].ymin = MIN(p.y, q.y);
    edges[ne].ymax = MAX(p.y, q.y);
    ne++;
  }

  y0 = y1 = 0;
  if (rc == result_OK && ne > 0)
  {
    qsort(edges, (size_t) ne, sizeof(*edges), edge_cmp);

    /* rows from the first crossed to one past the last, within the clip */
    y0 = MAX(edges[0].ymin, clip->y0);
    y1 = edges[0].ymax;
    for (i = 1; i < ne; i++)
      y1 = MAX(y1, edges[i].ymax);
    y1 = MIN(y1, clip->y1);
  }

  if (rc != result_OK || y0 >= y1)
  {
    free(edges);
    free(active);
    free(xs);
    return rc;
  }

  rc = region__builder_init(&b, y0, y1 - y0);

  na   = 0;
  next = 0;

  for (y = y0; rc == result_OK && y < y1; y++)
  {
    /* bring in edges which begin at or before this row, drop finished ones */
    while (next < ne && edges[next].ymin <= y)
      active[na++] = &edges[next++];

    for (i = j = 0; i < na; i++)
      if (active[i]->ymax > y)
        active[j++] = active[i];
    na = j;

    nx = 0;
    for (i = 0; i < na; i++)
    {
      p = active[i]->p;
      q = active[i]->q;
      t = p.x + (int) ((long) (y - p.y) * (q.x - p.x) / (q.y - p.y));

      for (j = nx; j > 0 && xs[j - 1] > t; j--)
        xs[j] = xs[j - 1];
      xs[j] = t;
      nx++;
    }

    region__builder_row(&b, y);

    /* even-odd: crossings pair up, the end pixel included */
    for (k = 0; rc == result_OK && k + 1 < nx; k += 2)
      rc = region__builder_add(&b, MAX(xs[k], clip->x0),
                               MIN(xs[k + 1] + 1, clip->x1));
  }

  free(edges);
  free(active);
  free(xs);

  if (rc != result_OK)
  {
    region__builder_abort(&b);
    return rc;
  }

  return region__builder_finish(&b, out);
}
