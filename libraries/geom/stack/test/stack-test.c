/* geom/stack/test/stack-test.c */

#include "base/utils.h"
#include "geom/box.h"

#include "geom/stack.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

/* Three flex-1 children in a 100px box divide 34/33/33, abutting exactly. */
static result_t test_flex_split(void)
{
  enum { ROOT, A, B, C, N };
  static const box_t   root = { 0, 0, 100, 10 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_HBOX, .parent = -1 },
    [A]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .flex = 1 },
    [B]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .flex = 1 },
    [C]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .flex = 1 },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[A].x0 != 0   || out[A].x1 != 34)
    return result_TEST_FAILED;
  if (out[B].x0 != 34  || out[B].x1 != 67)
    return result_TEST_FAILED;
  if (out[C].x0 != 67  || out[C].x1 != 100)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/* Two independent rows with label leaves fed the same fixed width line up
 * their field boxes at a common x0. */
static result_t test_aligned_labels(void)
{
  enum { ROOT, ROW1, LBL1, FLD1, ROW2, LBL2, FLD2, N };
  static const box_t   root = { 0, 0, 200, 40 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_VBOX, .parent = -1 },
    [ROW1] = { .kind = stack_KIND_HBOX, .parent = ROOT, .size = 20 },
    [LBL1] = { .kind = stack_KIND_LEAF, .parent = ROW1, .size = 40, .align = stack_ALIGN_END },
    [FLD1] = { .kind = stack_KIND_LEAF, .parent = ROW1, .flex = 1 },
    [ROW2] = { .kind = stack_KIND_HBOX, .parent = ROOT, .size = 20 },
    [LBL2] = { .kind = stack_KIND_LEAF, .parent = ROW2, .size = 40, .align = stack_ALIGN_END },
    [FLD2] = { .kind = stack_KIND_LEAF, .parent = ROW2, .flex = 1 },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[FLD1].x0 != out[FLD2].x0)
    return result_TEST_FAILED;
  if (out[FLD1].x0 != 40)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/* A parent index pointing forward (or at itself) is rejected. */
static result_t test_bad_tree(void)
{
  enum { ROOT, A, N };
  static const box_t   root = { 0, 0, 10, 10 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_HBOX, .parent = -1 },
    [A]    = { .kind = stack_KIND_LEAF, .parent = A },   /* self-referential */
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_STACK_BAD_TREE)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/* ----------------------------------------------------------------------- */

result_t stack_test(const char *resources)
{
  result_t err;

  NOT_USED(resources);

  err = test_flex_split();
  if (err != result_TEST_PASSED)
    return err;

  err = test_aligned_labels();
  if (err != result_TEST_PASSED)
    return err;

  err = test_bad_tree();
  if (err != result_TEST_PASSED)
    return err;

  return result_TEST_PASSED;
}
