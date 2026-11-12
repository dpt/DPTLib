/* geom/region/orient.c -- a region flipped or turned within an area */

#include "base/utils.h"

#include "impl.h"

result_t region_orient(const region_t *region,
                       orient_t        orient,
                       size2d_t        area,
                       region_t      **out)
{
  region__builder_t b;
  result_t          rc;
  box_t             whole;
  box_t             bounds;
  box_t             within;
  size2d_t          size;
  orient_t          back;
  int               ax, ay;
  int               bx, by;
  box_t             to;
  int               y, x;
  int               x0;
  int               sx, sy;
  int               inside;

  *out = NULL;

  if ((unsigned) orient >= orient__LIMIT)
    return result_BAD_ARG;

  if (region == NULL)
    return result_OK;

  whole  = (box_t) BOX_POS_SIZE(0, 0, area.w, area.h);
  bounds = region_bounds(region);
  if (box_intersection(&bounds, &whole, &within))
    return result_OK;

  /* the bounds' corners land where the opposite turn takes them from */
  size = orient_size(orient, area);
  back = orient == orient_ROTATE_CW  ? orient_ROTATE_CCW :
         orient == orient_ROTATE_CCW ? orient_ROTATE_CW  : orient;
  orient_source(back, size.w, size.h, within.x0, within.y0, &ax, &ay);
  orient_source(back, size.w, size.h, within.x1 - 1, within.y1 - 1, &bx, &by);
  to.x0 = MIN(ax, bx);
  to.y0 = MIN(ay, by);
  to.x1 = MAX(ax, bx) + 1;
  to.y1 = MAX(ay, by) + 1;

  rc = region__builder_init(&b, to.y0, to.y1 - to.y0);
  if (rc != result_OK)
    return rc;

  /* ponytail: a pixel test each, so O(area of the bounds); map runs
   * directly if big regions turn slowly (flips are easy, turns need a
   * column sweep) */
  for (y = to.y0; rc == result_OK && y < to.y1; y++)
  {
    region__builder_row(&b, y);

    x0 = -1;
    for (x = to.x0; rc == result_OK && x <= to.x1; x++)
    {
      inside = 0;
      if (x < to.x1)
      {
        orient_source(orient, area.w, area.h, x, y, &sx, &sy);
        inside = region_contains(region, sx, sy);
      }
      if (inside && x0 < 0)
      {
        x0 = x;
      }
      else if (!inside && x0 >= 0)
      {
        rc = region__builder_add(&b, x0, x);
        x0 = -1;
      }
    }
  }

  if (rc != result_OK)
  {
    region__builder_abort(&b);
    return rc;
  }

  return region__builder_finish(&b, out);
}
