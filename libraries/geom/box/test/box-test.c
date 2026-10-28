/* geom/box/test/box-test.c */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "base/types.h"
#include "base/utils.h"
#include "geom/box.h"

#include "geom/box.h"

#include "test/all-tests.h"

static result_t test_equal(void)
{
  static const box_t a = { 10, 20, 30, 40 };
  static const box_t b = { 10, 20, 30, 40 };
  static const box_t c = { 10, 20, 30, 41 };

  if (!box_equal(&a, &a) || !box_equal(&a, &b) || box_equal(&a, &c))
    return result_TEST_FAILED;

  return result_TEST_PASSED;
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
  if (!box_equal(&translated, &initial))
    return result_TEST_FAILED;

  box_translated(&initial, 3, 5, &translated);
  if (!box_equal(&translated, &expected))
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

static result_t test_list_add(void)
{
  static const box_t a      = {  0,  0, 10, 10 };
  static const box_t inside = {  2,  2,  4,  4 };
  static const box_t right  = { 10,  0, 20, 10 }; /* shares a's right edge */
  static const box_t ab     = {  0,  0, 20, 10 };
  static const box_t far1   = { 50, 50, 60, 60 };
  static const box_t far2   = { 70, 70, 80, 80 };
  static const box_t last   = { 50, 50, 80, 80 };
  static const box_t empty  = {  5,  5,  5,  5 };

  box_t list[2];
  int   n;

  n = 0;
  if (box_list_add(list, &n, 2, &a) || n != 1)
    return result_TEST_FAILED;

  /* covered and empty boxes change nothing */
  if (box_list_add(list, &n, 2, &inside) || n != 1)
    return result_TEST_FAILED;
  if (box_list_add(list, &n, 2, &empty) || n != 1)
    return result_TEST_FAILED;

  /* a shared edge merges into one exact box */
  if (box_list_add(list, &n, 2, &right) || n != 1 || !box_equal(&list[0], &ab))
    return result_TEST_FAILED;

  if (box_list_add(list, &n, 2, &far1) || n != 2)
    return result_TEST_FAILED;

  /* full: unions into the last entry and says so */
  if (!box_list_add(list, &n, 2, &far2) || n != 2 || !box_equal(&list[1], &last))
    return result_TEST_FAILED;

  /* full again: the two near boxes merge and the far one keeps its own
   * entry, instead of everything inflating into the last entry */
  {
    static const box_t near1  = {   0,   0,  10,  10 };
    static const box_t near2  = {  12,   0,  22,  10 };
    static const box_t near12 = {   0,   0,  22,  10 };
    static const box_t far3   = { 100, 100, 110, 110 };

    n = 0;
    box_list_add(list, &n, 2, &near1);
    box_list_add(list, &n, 2, &near2);
    if (n != 2 || !box_list_add(list, &n, 2, &far3) || n != 2)
      return result_TEST_FAILED;
    if (!((box_equal(&list[0], &near12) && box_equal(&list[1], &far3)) ||
          (box_equal(&list[1], &near12) && box_equal(&list[0], &far3))))
      return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

result_t box_test(const char *resources)
{
  typedef result_t (*boxtestfn)(void);

  static const boxtestfn tests[] =
  {
    test_equal,
    test_reset_empty,
    test_contains_box,
    test_contains_point,
    test_translated,
    test_list_add
  };

  result_t rc;
  size_t   i;
  int      nfailures;

  NOT_USED(resources);

  nfailures = 0;
  for (i = 0; i < NELEMS(tests); i++)
  {
    rc = tests[i]();
    if (rc != result_TEST_PASSED)
      nfailures++;
  }

  return (nfailures == 0) ? result_TEST_PASSED : result_TEST_FAILED;
}
