/* geom/hull/test/hull-test.c -- convex hull tests */

#include <stdio.h>

#include "base/result.h"
#include "base/utils.h"
#include "geom/hull.h"
#include "geom/point.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

#define CHECK(cond_) \
  do { if (!(cond_)) { printf("hull-test: %s:%d: %s\n", \
                              __FILE__, __LINE__, #cond_); \
                       return result_TEST_FAILED; } } while (0)

/* Non-zero if hull[0..n) equals want[0..n). */
static int same(const point_t *hull, const point_t *want, int n)
{
  int i;

  for (i = 0; i < n; i++)
    if (hull[i].x != want[i].x || hull[i].y != want[i].y)
      return 0;

  return 1;
}

result_t hull_test(const char *resources)
{
  /* a square with an interior point and one collinear with an edge */
  point_t       square[] = { { 2, 2 }, { 4, 4 }, { 0, 0 }, { 2, 0 },
                             { 0, 4 }, { 4, 0 } };
  const point_t square_hull[] = { { 0, 0 }, { 4, 0 }, { 4, 4 }, { 0, 4 },
                                  { 0, 0 } };
  point_t       line[] = { { 1, 1 }, { 2, 2 }, { 0, 0 } };
  const point_t line_hull[] = { { 0, 0 }, { 2, 2 }, { 0, 0 } };
  point_t       one[] = { { 3, 5 } };
  point_t       hull[2 * 6 + 1];

  NOT_USED(resources);

  printf("test: hull of a square\n");
  CHECK(hull_convex(square, NELEMS(square), hull) == (int) NELEMS(square_hull));
  CHECK(same(hull, square_hull, NELEMS(square_hull)));

  printf("test: hull of collinear points\n");
  CHECK(hull_convex(line, NELEMS(line), hull) == (int) NELEMS(line_hull));
  CHECK(same(hull, line_hull, NELEMS(line_hull)));

  printf("test: hull of one point, and of none\n");
  CHECK(hull_convex(one, 1, hull) == 2);
  CHECK(hull[0].x == 3 && hull[0].y == 5 && hull[1].x == 3 && hull[1].y == 5);
  CHECK(hull_convex(one, 0, hull) == 0);

  return result_TEST_PASSED;
}
