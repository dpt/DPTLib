/* geom/region/test/region-test.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/types.h"
#include "base/utils.h"
#include "geom/region.h"

#include "test/all-tests.h"

#define W 40
#define H 40

static const box_t clip = { 0, 0, W, H };

/* count of contained pixels over the clip box */
static int area(const region_t *r)
{
  int x, y, n = 0;

  for (y = 0; y < H; y++)
    for (x = 0; x < W; x++)
      n += region_contains(r, x, y) != 0;

  return n;
}

static result_t test_box(void)
{
  static const box_t b = { 3, 4, 10, 9 };
  region_t *r;
  box_t     bb;

  if (region_from_box(&b, &r) != result_OK || r == NULL)
    return result_TEST_FAILED;

  bb = region_bounds(r);
  if (bb.x0 != 3 || bb.y0 != 4 || bb.x1 != 10 || bb.y1 != 9 ||
      area(r) != 7 * 5 ||
      !region_contains(r, 3, 4) || region_contains(r, 10, 4) ||
      region_contains(r, 3, 9) || region_contains(r, 2, 5))
  {
    region_destroy(r);
    return result_TEST_FAILED;
  }

  region_destroy(r);
  return result_TEST_PASSED;
}

static result_t test_empty(void)
{
  static const box_t  e     = { 5, 5, 5, 9 };
  static const point_t line[] = { {0,5}, {10,5}, {20,5} };
  region_t *r = (region_t *) 1;

  if (region_from_box(&e, &r) != result_OK || r != NULL)
    return result_TEST_FAILED;

  if (region_from_polygon(line, 3, &clip, &r) != result_OK || r != NULL)
    return result_TEST_FAILED;

  if (region_contains(NULL, 0, 0))
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/* a concave 'L' polygon: rectangle 10x10 minus the top-right 5x5 */
static result_t test_polygon(void)
{
  static const point_t poly[] =
  { {2,2}, {7,2}, {7,7}, {12,7}, {12,12}, {2,12} };
  region_t *r;
  box_t     bb;
  int       a;

  if (region_from_polygon(poly, 6, &clip, &r) != result_OK || r == NULL)
    return result_TEST_FAILED;

  bb = region_bounds(r);
  a  = area(r);
  /* edges at x=7 and y=7 cross at the right-hand pixel, so the exact count
   * is whatever the even-odd inclusive rule gives; check shape instead */
  if (bb.x0 != 2 || bb.y0 != 2 || bb.y1 != 12 || bb.x1 != 13 ||
      !region_contains(r, 3, 3) || !region_contains(r, 10, 10) ||
      region_contains(r, 10, 3) || region_contains(r, 1, 5) || a < 50)
  {
    printf("polygon: bounds %d,%d %d,%d area %d\n",
           bb.x0, bb.y0, bb.x1, bb.y1, a);
    region_destroy(r);
    return result_TEST_FAILED;
  }

  region_destroy(r);
  return result_TEST_PASSED;
}

static result_t test_polygon_clip(void)
{
  static const point_t poly[] = { {-20,-20}, {60,-20}, {60,60}, {-20,60} };
  region_t *r;
  box_t     bb;

  if (region_from_polygon(poly, 4, &clip, &r) != result_OK || r == NULL)
    return result_TEST_FAILED;

  bb = region_bounds(r);
  if (bb.x0 != 0 || bb.y0 != 0 || bb.x1 != W || bb.y1 != H ||
      area(r) != W * H)
  {
    region_destroy(r);
    return result_TEST_FAILED;
  }

  region_destroy(r);
  return result_TEST_PASSED;
}

/* the outline spans must cover exactly the pixels with an outside neighbour */
static result_t outline_matches(const region_t *r)
{
  static const int dx[] = { -1, 1, 0, 0 };
  static const int dy[] = { 0, 0, -1, 1 };
  unsigned char *got, *want;
  box_t         *spans;
  int            n, i, x, y, k, bad = 0;

  if (region_outline(r, &spans, &n) != result_OK)
    return result_TEST_FAILED;

  got  = calloc(W * H, 1);
  want = calloc(W * H, 1);

  for (i = 0; i < n; i++)
    for (x = spans[i].x0; x < spans[i].x1; x++)
      got[spans[i].y0 * W + x] = 1;

  for (y = 0; y < H; y++)
    for (x = 0; x < W; x++)
      if (region_contains(r, x, y))
        for (k = 0; k < 4; k++)
          if (!region_contains(r, x + dx[k], y + dy[k]))
            want[y * W + x] = 1;

  bad = memcmp(got, want, W * H) != 0;

  free(got);
  free(want);
  free(spans);

  return bad ? result_TEST_FAILED : result_TEST_PASSED;
}

static result_t test_outline(void)
{
  static const point_t poly[] =
  { {5,3}, {30,10}, {12,20}, {35,35}, {8,28}, {15,12} };
  static const box_t   b = { 4, 4, 20, 15 };
  region_t *r;
  result_t  rc;

  if (region_from_box(&b, &r) != result_OK)
    return result_TEST_FAILED;
  rc = outline_matches(r);
  region_destroy(r);
  if (rc != result_TEST_PASSED)
    return rc;

  if (region_from_polygon(poly, 6, &clip, &r) != result_OK || r == NULL)
    return result_TEST_FAILED;
  rc = outline_matches(r);
  region_destroy(r);

  return rc;
}

static result_t test_invert(void)
{
  static const box_t b = { 4, 4, 20, 15 };
  region_t *r, *inv, *back = NULL;
  box_t     bb;
  int       ok;

  if (region_from_box(&b, &r) != result_OK)
    return result_TEST_FAILED;

  if (region_invert(r, &clip, &inv) != result_OK || inv == NULL)
    return result_TEST_FAILED;

  ok = area(inv) == W * H - 16 * 11 &&
       !region_contains(inv, 4, 4) && region_contains(inv, 3, 4) &&
       region_contains(inv, 20, 14) && region_contains(inv, 10, 15);

  /* inverting twice gives the original */
  ok = ok && region_invert(inv, &clip, &back) == result_OK && back != NULL;
  if (ok)
  {
    bb = region_bounds(back);
    ok = bb.x0 == 4 && bb.y0 == 4 && bb.x1 == 20 && bb.y1 == 15 &&
         area(back) == 16 * 11;
  }

  /* the complement of everything, and of nothing */
  region_destroy(back);
  region_destroy(inv);
  region_destroy(r);

  if (region_from_box(&clip, &r) != result_OK ||
      region_invert(r, &clip, &inv) != result_OK || inv != NULL)
    ok = 0;
  region_destroy(r);

  if (region_invert(NULL, &clip, &inv) != result_OK || inv == NULL ||
      area(inv) != W * H)
    ok = 0;
  region_destroy(inv);

  return ok ? result_TEST_PASSED : result_TEST_FAILED;
}

result_t region_test(const char *resources)
{
  typedef result_t (*regiontestfn)(void);

  static const regiontestfn tests[] =
  {
    test_box,
    test_empty,
    test_polygon,
    test_polygon_clip,
    test_outline,
    test_invert
  };

  size_t i;

  NOT_USED(resources);

  for (i = 0; i < NELEMS(tests); i++)
  {
    if (tests[i]() != result_TEST_PASSED)
    {
      printf("region test #%zu failed\n", i);
      return result_TEST_FAILED;
    }
  }

  return result_TEST_PASSED;
}
