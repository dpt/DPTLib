
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "base/types.h"
#include "base/utils.h"
#include "geom/box.h"

#include "geom/box.h"

#include "test/all-tests.h"

static int boxeq(const box_t *a, const box_t *b)
{
  return a->x0 == b->x0 && a->y0 == b->y0 &&
         a->x1 == b->x1 && a->y1 == b->y1;
}

static result_t test_reset_empty(void)
{
  box_t reset;

  box_reset(&reset);
  return box_is_empty(&reset) ? result_TEST_PASSED : result_TEST_FAILED;
}

static result_t test_contains_box(void)
{
  static const box_t outer         = { 10, 10, 12, 12 }; // 2x2 box
  static const box_t contained     = { 10, 10, 11, 11 }; // 1x1 box
  static const box_t not_contained = { 10, 10, 11, 13 }; // 1x3 box

  if (!box_contains_box(&outer, &outer))
    return result_TEST_FAILED;

  if (!box_contains_box(&contained, &outer))
    return result_TEST_FAILED;

  if (box_contains_box(&not_contained, &outer))
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

static result_t test_contains_point(void)
{
  typedef struct { int x,y; } point_t;

  static const box_t   b            = { 10, 10, 12, 12 }; // 2x2 box
  static const point_t goodpoints[] = { {10,10}, {11,11} };
  static const point_t badpoints[]  = { {9,9}, {12,12} };

  size_t i;

  for (i = 0; i < NELEMS(goodpoints); i++)
    if (!box_contains_point(&b, goodpoints[i].x, goodpoints[i].y))
    {
      printf("box_contains_point() failed good point #%zu\n", i);
      return result_TEST_FAILED;
    }

  for (i = 0; i < NELEMS(badpoints); i++)
    if (box_contains_point(&b, badpoints[i].x, badpoints[i].y))
    {
      printf("box_contains_point() failed bad point #%zu\n", i);
      return result_TEST_FAILED;
    }

  return result_TEST_PASSED;
}

static result_t test_translated(void)
{
  static const box_t initial  = { 10, 20, 30, 40 };
  static const box_t expected = { 13, 25, 33, 45 };

  box_t translated;

  box_translated(&initial, 0, 0, &translated);
  if (!boxeq(&translated, &initial))
    return result_TEST_FAILED;

  box_translated(&initial, 3, 5, &translated);
  if (!boxeq(&translated, &expected))
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

result_t box_test(const char *resources)
{
  typedef result_t (*boxtestfn)(void);

  static const boxtestfn tests[] =
  {
    test_reset_empty,
    test_contains_box,
    test_contains_point,
    test_translated
  };

  result_t rc;
  size_t   i;
  int      nfailures;

  nfailures = 0;
  for (i = 0; i < NELEMS(tests); i++)
  {
    rc = tests[i]();
    if (rc != result_TEST_PASSED)
      nfailures++;
  }

  return (nfailures == 0) ? result_TEST_PASSED : result_TEST_FAILED;
}
