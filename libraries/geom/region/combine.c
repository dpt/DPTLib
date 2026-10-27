/* geom/region/combine.c -- union and difference of regions */

#include "base/utils.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

typedef enum
{
  combine_UNION,
  combine_SUBTRACT
}
combine_t;

/* add one scanline's worth of a op b, given each one's runs on it */
static result_t combine_row(region__builder_t *bld,
                            combine_t          op,
                            const int         *ra,
                            int                na,
                            const int         *rb,
                            int                nb)
{
  result_t rc;
  int      i, j, k, x;

  rc = result_OK;

  if (op == combine_UNION)
  {
    /* merge by start: the builder joins runs that touch or overlap */
    i = 0;
    j = 0;
    while (rc == result_OK && (i < na || j < nb))
    {
      if (j == nb || (i < na && ra[i * 2] <= rb[j * 2]))
      {
        rc = region__builder_add(bld, ra[i * 2], ra[i * 2 + 1]);
        i++;
      }
      else
      {
        rc = region__builder_add(bld, rb[j * 2], rb[j * 2 + 1]);
        j++;
      }
    }

    return rc;
  }

  /* each run of a, less the runs of b it overlaps. empty pieces are ignored
   * by the builder */
  j = 0;
  for (i = 0; rc == result_OK && i < na; i++)
  {
    x = ra[i * 2];
    while (j < nb && rb[j * 2 + 1] <= x)
      j++;

    for (k = j; rc == result_OK && k < nb && rb[k * 2] < ra[i * 2 + 1]; k++)
    {
      rc = region__builder_add(bld, x, rb[k * 2]);
      x  = MAX(x, rb[k * 2 + 1]);
    }

    if (rc == result_OK)
      rc = region__builder_add(bld, x, ra[i * 2 + 1]);
  }

  return rc;
}

static result_t combine(const region_t *a,
                        const region_t *b,
                        combine_t       op,
                        region_t      **out)
{
  result_t          rc;
  box_t             ab, bb;
  int               y0, y1;
  region__builder_t bld;
  int               y;
  const int        *ra, *rb;
  int               na, nb;

  *out = NULL;

  if (a == NULL && (op == combine_SUBTRACT || b == NULL))
    return result_OK;

  ab = region_bounds(a);
  bb = region_bounds(b);

  if (a == NULL)
  {
    y0 = bb.y0;
    y1 = bb.y1;
  }
  else if (b == NULL || op == combine_SUBTRACT)
  {
    y0 = ab.y0;
    y1 = ab.y1;
  }
  else
  {
    y0 = MIN(ab.y0, bb.y0);
    y1 = MAX(ab.y1, bb.y1);
  }

  rc = region__builder_init(&bld, y0, y1 - y0);
  if (rc != result_OK)
    return rc;

  for (y = y0; rc == result_OK && y < y1; y++)
  {
    region__builder_row(&bld, y);

    na = region_row(a, y, &ra);
    nb = region_row(b, y, &rb);
    rc = combine_row(&bld, op, ra, na, rb, nb);
  }

  if (rc != result_OK)
  {
    region__builder_abort(&bld);
    return rc;
  }

  return region__builder_finish(&bld, out);
}

/* ----------------------------------------------------------------------- */

result_t region_union(const region_t *a, const region_t *b, region_t **out)
{
  return combine(a, b, combine_UNION, out);
}

result_t region_subtract(const region_t *a,
                         const region_t *b,
                         region_t      **out)
{
  return combine(a, b, combine_SUBTRACT, out);
}
