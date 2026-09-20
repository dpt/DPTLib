/* geom/stack/test/stack-test.c */

#include "base/utils.h"
#include "geom/box.h"

#include "geom/stack.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

/*   0        34    67        100
 *   +---------+-----+---------+
 *   |  A f=1  | B f1|  C f=1  |
 *   +---------+-----+---------+
 *
 * Three flex-1 children in a 100px box divide 34/33/33, abutting exactly. */
static result_t test_flex_split(void)
{
  enum { ROOT, A, B, C, N };
  static const box_t        root = { 0, 0, 100, 10 };
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

/*   0          40                    200
 *   +----------+----------------------+
 *   | LBL1(40) | FLD1 flex=1          |  ROW1 (h=20)
 *   +----------+----------------------+
 *   | LBL2(40) | FLD2 flex=1          |  ROW2 (h=20)
 *   +----------+----------------------+
 *
 * Two independent rows with label leaves fed the same fixed width line up
 * their field boxes at a common x0. */
static result_t test_aligned_labels(void)
{
  enum { ROOT, ROW1, LBL1, FLD1, ROW2, LBL2, FLD2, N };
  static const box_t        root = { 0, 0, 200, 40 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_VBOX, .parent = -1 },
    
    [ROW1] = { .kind = stack_KIND_HBOX, .parent = ROOT, .axis_size = 20 },
    [LBL1] = { .kind = stack_KIND_LEAF, .parent = ROW1, .axis_size = 40, .align = stack_ALIGN_END },
    [FLD1] = { .kind = stack_KIND_LEAF, .parent = ROW1, .flex = 1 },

    [ROW2] = { .kind = stack_KIND_HBOX, .parent = ROOT, .axis_size = 20 },
    [LBL2] = { .kind = stack_KIND_LEAF, .parent = ROW2, .axis_size = 40, .align = stack_ALIGN_END },
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

/*   0,0 +--------+ 10,0
 *       |   A    |  flex=1   y: 0..50
 *       +--------+
 *       |   B    |  flex=1   y: 50..100
 *       +--------+
 *   0,100        10,100
 *
 * A VBOX splits its children top to bottom, cross axis is x not y. */
static result_t test_vbox_flex_split(void)
{
  enum { ROOT, A, B, N };
  static const box_t        root = { 0, 0, 10, 100 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_VBOX, .parent = -1 },
    [A]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .flex = 1 },
    [B]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .flex = 1 },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[A].y0 != 0  || out[A].y1 != 50)
    return result_TEST_FAILED;
  if (out[B].y0 != 50 || out[B].y1 != 100)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/*   0         45  gap=10  55        100
 *   +---------+   ....   +---------+
 *   | A flex=1|   ....   | B flex=1|
 *   +---------+   ....   +---------+
 *
 * A gap is inserted between adjacent children but not before the first or
 * after the last. */
static result_t test_gap(void)
{
  enum { ROOT, A, B, N };
  static const box_t        root = { 0, 0, 100, 10 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_HBOX, .parent = -1, .gap = 10 },
    [A]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .flex = 1 },
    [B]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .flex = 1 },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[A].x0 != 0  || out[A].x1 != 45)
    return result_TEST_FAILED;
  if (out[B].x0 != 55 || out[B].x1 != 100)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/*   0,0 +------------------------+ 100,0
 *       |        pad_t=6         |
 *       |   +----------------+   |
 *       |pl |     A (fill)   |pr |   pad_l=5, pad_r=7
 *       |   +----------------+   |
 *       |        pad_b=8         |
 *       +------------------------+
 *   0,100                    100,100
 *
 * Container padding insets the area available to its children on every
 * edge. */
static result_t test_padding(void)
{
  enum { ROOT, A, N };
  static const box_t        root = { 0, 0, 100, 100 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_HBOX, .parent = -1,
               .pad_l = 5, .pad_t = 6, .pad_r = 7, .pad_b = 8 },
    [A]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .flex = 1,
               .align = stack_ALIGN_FILL },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[A].x0 != 5  || out[A].x1 != 93)
    return result_TEST_FAILED;
  if (out[A].y0 != 6  || out[A].y1 != 92)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/*   0                   80        100
 *   +-------------------+---------+
 *   |     A min=80      | B flex=1|
 *   +-------------------+---------+
 *
 * A child's 'min' floors its extent below what an even flex split would
 * give it; the remaining flex child absorbs the rest. */
static result_t test_min_clamp(void)
{
  enum { ROOT, A, B, N };
  static const box_t        root = { 0, 0, 100, 10 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_HBOX, .parent = -1 },
    [A]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .min = 80 },
    [B]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .flex = 1 },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[A].x1 - out[A].x0 != 80)
    return result_TEST_FAILED;
  if (out[B].x1 - out[B].x0 != 20)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/*   0        20                    100
 *   +--------+----------------------+
 *   |A flex=1|   B flex=1 (absorbs  |
 *   |max=20  |   the excess slack)  |
 *   +--------+----------------------+
 *
 * A child's 'max' ceilings the space handed to it by flex growth; the
 * excess slack is returned to its sibling. */
static result_t test_max_clamp(void)
{
  enum { ROOT, A, B, N };
  static const box_t        root = { 0, 0, 100, 10 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_HBOX, .parent = -1 },
    [A]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .flex = 1, .max = 20 },
    [B]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .flex = 1 },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[A].x1 - out[A].x0 != 20)
    return result_TEST_FAILED;
  if (out[B].x1 - out[B].x0 != 80)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/*   0      10                  90      100
 *   +------+--------------------+------+
 *   |A sz10|   SPACER flex=1    |B sz10|
 *   +------+--------------------+------+
 *
 * A SPACER consumes main-axis space like a leaf but is otherwise just
 * another flexible child. */
static result_t test_spacer(void)
{
  enum { ROOT, A, SPACER, B, N };
  static const box_t        root = { 0, 0, 100, 10 };
  static const stack_item_t items[N] =
  {
    [ROOT]   = { .kind = stack_KIND_HBOX,   .parent = -1 },
    [A]      = { .kind = stack_KIND_LEAF,   .parent = ROOT, .axis_size = 10 },
    [SPACER] = { .kind = stack_KIND_SPACER, .parent = ROOT, .flex = 1 },
    [B]      = { .kind = stack_KIND_LEAF,   .parent = ROOT, .axis_size = 10 },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[A].x0 != 0  || out[A].x1 != 10)
    return result_TEST_FAILED;
  if (out[B].x0 != 90 || out[B].x1 != 100)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/*   y=0  +----------------------+
 *        |                      |
 *        |          A           |  fills full 40px cross extent
 *        |                      |
 *   y=40 +----------------------+
 *
 * ALIGN_FILL spans the full cross extent of the container. */
static result_t test_align_fill(void)
{
  enum { ROOT, A, N };
  static const box_t        root = { 0, 0, 100, 40 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_HBOX, .parent = -1 },
    [A]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .flex = 1,
               .align = stack_ALIGN_FILL },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[A].y0 != 0 || out[A].y1 != 40)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/*   y=0  +----------------------+
 *        |                      |
 *   y=15 |     +---A(10)---+    |
 *        |     +-----------+    |
 *   y=25 |                      |
 *   y=40 +----------------------+
 *
 * ALIGN_CENTRE centres a sized child within the container's cross
 * extent. */
static result_t test_align_centre(void)
{
  enum { ROOT, A, N };
  static const box_t        root = { 0, 0, 100, 40 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_HBOX, .parent = -1 },
    [A]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .flex = 1,
               .cross_size = 10, .align = stack_ALIGN_CENTRE },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[A].y0 != 15 || out[A].y1 != 25)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/*   0,0                          100,0
 *   +------------------------------+
 *   |      ROW (HBOX, fill)        |
 *   | +------------+-------------+ |
 *   | |  A flex=1  |  B flex=1   | |
 *   | +------------+-------------+ |
 *   +------------------------------+
 *   0,50                        100,50
 *
 * An HBOX nested inside a VBOX is itself placed and then lays out its own
 * children, recursing correctly through mixed axes. */
static result_t test_nested_containers(void)
{
  enum { ROOT, ROW, A, B, N };
  static const box_t        root = { 0, 0, 100, 50 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_VBOX, .parent = -1 },
    [ROW]  = { .kind = stack_KIND_HBOX, .parent = ROOT, .flex = 1,
               .align = stack_ALIGN_FILL },
    [A]    = { .kind = stack_KIND_LEAF, .parent = ROW,  .flex = 1 },
    [B]    = { .kind = stack_KIND_LEAF, .parent = ROW,  .flex = 1 },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[ROW].x0 != 0 || out[ROW].x1 != 100)
    return result_TEST_FAILED;
  if (out[A].x0 != 0   || out[A].x1 != 50)
    return result_TEST_FAILED;
  if (out[B].x0 != 50  || out[B].x1 != 100)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/*   1,2  +------+  3,2
 *        | ROOT |       no children
 *        +------+
 *   1,4            3,4
 *
 * A tree of just the root, with no children, solves trivially to the root
 * box itself. */
static result_t test_root_only(void)
{
  enum { ROOT, N };
  static const box_t        root = { 1, 2, 3, 4 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_HBOX, .parent = -1 },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[ROOT].x0 != 1 || out[ROOT].y0 != 2 ||
      out[ROOT].x1 != 3 || out[ROOT].y1 != 4)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/*   ROOT
 *    |
 *    A ---parent---> A   (self-loop, invalid)
 *
 * A parent index pointing forward (or at itself) is rejected. */
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

/*   ROOT (VBOX, pad 2 all round)
 *    |
 *    +-- ROW (HBOX, gap=3)
 *         +-- LBL  min=20
 *         +-- FLD  flex=1, min=15
 *
 * ROW's minimum main axis is 20+3+15=38; ROOT's minimum is that plus its own
 * padding (2+2=4) on each axis -- width 42, height (ROW has no explicit
 * axis_size/min, so 0) 4. Measuring, then solving into exactly that size,
 * must leave every leaf at its minimum (no slack for FLD's flex to grow
 * into). */
static result_t test_measure_min(void)
{
  enum { ROOT, ROW, LBL, FLD, N };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_VBOX, .parent = -1,
               .pad_l = 2, .pad_t = 2, .pad_r = 2, .pad_b = 2 },
    [ROW]  = { .kind = stack_KIND_HBOX, .parent = ROOT, .gap = 3 },
    [LBL]  = { .kind = stack_KIND_LEAF, .parent = ROW, .min = 20 },
    [FLD]  = { .kind = stack_KIND_LEAF, .parent = ROW, .flex = 1, .min = 15 },
  };

  result_t err;
  size2d_t sz;
  box_t    root, out[N];

  err = stack_smallest(items, N, &sz);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (sz.w != 42 || sz.h != 4)
    return result_TEST_FAILED;

  root = (box_t) BOX_POS_SIZE(0, 0, sz.w, sz.h);
  err  = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[LBL].x1 - out[LBL].x0 != 20)
    return result_TEST_FAILED;
  if (out[FLD].x1 - out[FLD].x0 != 15)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/*   ROOT (VBOX)
 *    |
 *    +-- HUG (VBOX, axis_size=STACK_HUG, pad_t=2, pad_b=3)
 *         +-- A  axis_size=10
 *         +-- B  axis_size=15
 *        (gap=4)
 *
 * HUG's main axis sizes to its children: 10+4+15=29, plus its own
 * pad_t/pad_b (2+3=5), giving it a height of 34 even though nothing in the
 * tree says so directly. */
static result_t test_hug(void)
{
  enum { ROOT, HUG, A, B, N };
  static const box_t        root = { 0, 0, 50, 100 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_VBOX, .parent = -1 },
    [HUG]  = { .kind = stack_KIND_VBOX, .parent = ROOT, .axis_size = STACK_HUG,
               .gap = 4, .pad_t = 2, .pad_b = 3, .align = stack_ALIGN_FILL },
    [A]    = { .kind = stack_KIND_LEAF, .parent = HUG, .axis_size = 10 },
    [B]    = { .kind = stack_KIND_LEAF, .parent = HUG, .axis_size = 15 },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[HUG].y1 - out[HUG].y0 != 34)
    return result_TEST_FAILED;
  if (out[A].y0 != out[HUG].y0 + 2 || out[A].y1 - out[A].y0 != 10)
    return result_TEST_FAILED;
  if (out[B].y0 != out[A].y1 + 4   || out[B].y1 - out[B].y0 != 15)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/*   ROOT (VBOX)
 *    |
 *    +-- HUG (VBOX, axis_size=STACK_HUG)
 *         +-- A  flex=1  (no axis_size/min -- falls back to 0)
 *
 * A flexible child inside a hugging container has no slack to grow into
 * (hug never hands out spare space), so it contributes its 'min' -- 0 here
 * -- to the sum, same as stack_smallest's own min-content measurement. */
static result_t test_hug_flex_child_falls_back_to_min(void)
{
  enum { ROOT, HUG, A, N };
  static const box_t        root = { 0, 0, 50, 100 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_VBOX, .parent = -1 },
    [HUG]  = { .kind = stack_KIND_VBOX, .parent = ROOT, .axis_size = STACK_HUG },
    [A]    = { .kind = stack_KIND_LEAF, .parent = HUG, .flex = 1, .min = 7 },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_OK)
    return result_TEST_FAILED;

  if (out[HUG].y1 - out[HUG].y0 != 7)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/* STACK_HUG on the root is rejected -- the root's size comes from the
 * caller (or stack_smallest), not from the tree. */
static result_t test_hug_on_root_rejected(void)
{
  enum { ROOT, N };
  static const box_t        root = { 0, 0, 50, 50 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_VBOX, .parent = -1, .axis_size = STACK_HUG },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_STACK_BAD_TREE)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/* STACK_HUG on a leaf is rejected -- there are no children to hug around. */
static result_t test_hug_on_leaf_rejected(void)
{
  enum { ROOT, A, N };
  static const box_t        root = { 0, 0, 50, 50 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_VBOX, .parent = -1 },
    [A]    = { .kind = stack_KIND_LEAF, .parent = ROOT, .axis_size = STACK_HUG },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_STACK_BAD_TREE)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/* STACK_HUG on a spacer is rejected, same reasoning as a leaf. */
static result_t test_hug_on_spacer_rejected(void)
{
  enum { ROOT, A, N };
  static const box_t        root = { 0, 0, 50, 50 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_VBOX, .parent = -1 },
    [A]    = { .kind = stack_KIND_SPACER, .parent = ROOT, .axis_size = STACK_HUG },
  };

  result_t err;
  box_t    out[N];

  err = stack_solve(items, N, &root, out);
  if (err != result_STACK_BAD_TREE)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/* STACK_HUG combined with a non-zero flex on the same item is rejected --
 * hug is an absolute size, not something flex can grow further. */
static result_t test_hug_with_flex_rejected(void)
{
  enum { ROOT, HUG, N };
  static const box_t        root = { 0, 0, 50, 50 };
  static const stack_item_t items[N] =
  {
    [ROOT] = { .kind = stack_KIND_VBOX, .parent = -1 },
    [HUG]  = { .kind = stack_KIND_VBOX, .parent = ROOT,
               .axis_size = STACK_HUG, .flex = 1 },
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

  err = test_vbox_flex_split();
  if (err != result_TEST_PASSED)
    return err;

  err = test_gap();
  if (err != result_TEST_PASSED)
    return err;

  err = test_padding();
  if (err != result_TEST_PASSED)
    return err;

  err = test_min_clamp();
  if (err != result_TEST_PASSED)
    return err;

  err = test_max_clamp();
  if (err != result_TEST_PASSED)
    return err;

  err = test_spacer();
  if (err != result_TEST_PASSED)
    return err;

  err = test_align_fill();
  if (err != result_TEST_PASSED)
    return err;

  err = test_align_centre();
  if (err != result_TEST_PASSED)
    return err;

  err = test_nested_containers();
  if (err != result_TEST_PASSED)
    return err;

  err = test_root_only();
  if (err != result_TEST_PASSED)
    return err;

  err = test_bad_tree();
  if (err != result_TEST_PASSED)
    return err;

  err = test_measure_min();
  if (err != result_TEST_PASSED)
    return err;

  err = test_hug();
  if (err != result_TEST_PASSED)
    return err;

  err = test_hug_flex_child_falls_back_to_min();
  if (err != result_TEST_PASSED)
    return err;

  err = test_hug_on_root_rejected();
  if (err != result_TEST_PASSED)
    return err;

  err = test_hug_on_leaf_rejected();
  if (err != result_TEST_PASSED)
    return err;

  err = test_hug_on_spacer_rejected();
  if (err != result_TEST_PASSED)
    return err;

  err = test_hug_with_flex_rejected();
  if (err != result_TEST_PASSED)
    return err;

  return result_TEST_PASSED;
}
