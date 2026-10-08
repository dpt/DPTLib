/* geom/region/polygon.c -- build a region from a polygon (even-odd fill) */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

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

/* growable run list for the scanlines built so far */
typedef struct
{
  int *runs;
  int  n, cap;
}
runlist_t;

static result_t runlist_add(runlist_t *l, int x0, int x1)
{
  int *p;

  if (l->n == l->cap)
  {
    l->cap = l->cap ? l->cap * 2 : 64;
    p      = realloc(l->runs, (size_t) l->cap * sizeof(*p));
    if (p == NULL)
      return result_OOM;

    l->runs = p;
  }

  l->runs[l->n++] = x0;
  l->runs[l->n++] = x1;
  return result_OK;
}

result_t region_from_polygon(const point_t *pts,
                             int            n,
                             const box_t   *clip,
                             region_t     **out)
{
  result_t  rc;
  edge_t   *edges;
  edge_t  **active;
  int      *xs;
  int      *start;
  runlist_t l;
  region_t *r;
  int       ne, na, next, nx, rowstart, h;
  int       y, y0, y1, i, j, k, t, a, b, first, last;
  point_t   p, q;
  int       xmin, xmax;

  *out = NULL;

  if (n < 3 || box_is_empty(clip))
    return result_OK;

  edges  = malloc((size_t) n * sizeof(*edges));
  active = malloc((size_t) n * sizeof(*active));
  xs     = malloc((size_t) n * sizeof(*xs));
  if (edges == NULL || active == NULL || xs == NULL)
  {
    free(edges);
    free(active);
    free(xs);
    return result_OOM;
  }

  /* horizontal edges never cross a scanline centre */
  ne = 0;
  for (i = 0; i < n; i++)
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

  if (ne == 0)
  {
    free(edges);
    free(active);
    free(xs);
    return result_OK;
  }

  qsort(edges, (size_t) ne, sizeof(*edges), edge_cmp);

  /* rows from the first crossed to one past the last, within the clip */
  y0 = MAX(edges[0].ymin, clip->y0);
  y1 = edges[0].ymax;
  for (i = 1; i < ne; i++)
    y1 = MAX(y1, edges[i].ymax);
  y1 = MIN(y1, clip->y1);

  if (y0 >= y1)
  {
    free(edges);
    free(active);
    free(xs);
    return result_OK;
  }

  h     = y1 - y0;
  start = malloc((size_t) (h + 1) * sizeof(*start));
  l     = (runlist_t) { NULL, 0, 0 };
  rc    = start == NULL ? result_OOM : result_OK;

  na   = 0;
  next = 0;
  first = -1;
  last  = -1;
  xmin  = INT_MAX;
  xmax  = INT_MIN;

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

    start[y - y0] = l.n;
    rowstart      = l.n;

    for (k = 0; rc == result_OK && k + 1 < nx; k += 2)
    {
      a = MAX(xs[k], clip->x0);
      b = MIN(xs[k + 1] + 1, clip->x1);
      if (a >= b)
        continue;

      /* merge with the previous run on this row if touching */
      if (l.n > rowstart && a <= l.runs[l.n - 1])
      {
        l.runs[l.n - 1] = MAX(l.runs[l.n - 1], b);
        continue;
      }

      rc = runlist_add(&l, a, b);
    }

    if (l.n > rowstart)
    {
      if (first < 0)
        first = y - y0;
      last = y - y0;
      xmin = MIN(xmin, l.runs[rowstart]);
      xmax = MAX(xmax, l.runs[l.n - 1]);
    }
  }

  free(edges);
  free(active);
  free(xs);

  if (rc != result_OK || first < 0)
  {
    free(start);
    free(l.runs);
    return rc;
  }

  r = malloc(sizeof(*r));
  if (r == NULL)
  {
    free(start);
    free(l.runs);
    return result_OOM;
  }

  /* trim empty leading and trailing rows */
  start[h] = l.n;
  if (first > 0)
    memmove(start, start + first,
            (size_t) (last - first + 1) * sizeof(*start));
  start[last - first + 1] = l.n;
  r->start = start;
  r->runs  = l.runs;

  r->bounds.x0 = xmin;
  r->bounds.x1 = xmax;
  r->bounds.y0 = y0 + first;
  r->bounds.y1 = y0 + last + 1;

  *out = r;
  return result_OK;
}
