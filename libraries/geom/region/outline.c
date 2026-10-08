/* geom/region/outline.c -- boundary pixels of a region */

#include <stdlib.h>

#include "base/utils.h"

#include "geom/region.h"
#include "utils/array.h"

#include "impl.h"

typedef struct
{
  box_t *spans;
  int    n, cap;
}
spanlist_t;

static result_t spans_add(spanlist_t *l, int x0, int x1, int y)
{
  if (x0 >= x1)
    return result_OK;

  if (array_grow((void **) &l->spans, sizeof(*l->spans), l->n, &l->cap, 1, 64))
    return result_OOM;

  l->spans[l->n++] = (box_t) { x0, y, x1, y + 1 };
  return result_OK;
}

/* add the parts of [a,b) on row y not covered by the neighbouring row's runs */
static result_t add_uncovered(spanlist_t *l,
                              int         a,
                              int         b,
                              int         y,
                              const int  *nruns,
                              int         nn)
{
  result_t rc;
  int      i;

  for (i = 0; i < nn && a < b; i++)
  {
    if (nruns[i * 2 + 1] <= a)
      continue;

    if (nruns[i * 2] >= b)
      break;

    rc = spans_add(l, a, MIN(nruns[i * 2], b), y);
    if (rc != result_OK)
      return rc;

    a = MAX(a, nruns[i * 2 + 1]);
  }

  return spans_add(l, a, b, y);
}

result_t region_outline(const region_t *region, box_t **spans, int *n)
{
  result_t   rc;
  spanlist_t l = { NULL, 0, 0 };
  const int *runs, *up, *down;
  int        y, i, nr, nu, nd;

  *spans = NULL;
  *n     = 0;

  if (region == NULL)
    return result_OK;

  rc = result_OK;

  for (y = region->bounds.y0; rc == result_OK && y < region->bounds.y1; y++)
  {
    nr = region_row(region, y, &runs);
    nu = region_row(region, y - 1, &up);
    nd = region_row(region, y + 1, &down);

    for (i = 0; rc == result_OK && i < nr; i++)
    {
      /* the ends have a horizontal neighbour outside */
      rc = spans_add(&l, runs[i * 2], runs[i * 2] + 1, y);
      if (rc == result_OK)
        rc = spans_add(&l, runs[i * 2 + 1] - 1, runs[i * 2 + 1], y);
      if (rc == result_OK)
        rc = add_uncovered(&l, runs[i * 2], runs[i * 2 + 1], y, up, nu);
      if (rc == result_OK)
        rc = add_uncovered(&l, runs[i * 2], runs[i * 2 + 1], y, down, nd);
    }
  }

  if (rc != result_OK)
  {
    free(l.spans);
    return rc;
  }

  *spans = l.spans;
  *n     = l.n;
  return result_OK;
}
