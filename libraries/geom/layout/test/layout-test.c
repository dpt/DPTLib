/* geom/layout/test/layout-test.c */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "base/utils.h"
#include "geom/box.h"
#include "test/txtscr.h"

#include "geom/layout.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

/* Lays out elements in a fresh packer over dims, with spacing and leading
 * both 'gap'. */
static result_t run(const box_t            *dims,
                    int                     gap,
                    const layout_element_t *elements,
                    int                     nelements,
                    box_t                  *boxes,
                    int                     nboxes)
{
  result_t      err;
  packer_t     *packer;
  layout_spec_t spec;

  packer = packer_create(dims);
  if (packer == NULL)
    return result_OOM;

  spec.packer  = packer;
  spec.loc     = packer_LOC_TOP_LEFT;
  spec.clear   = packer_CLEAR_LEFT;
  spec.spacing = gap;
  spec.leading = gap;

  err = layout_place(&spec, elements, nelements, boxes, nboxes);

  packer_destroy(packer);

  return err;
}

static int same_box(const box_t *got, int x0, int y0, int x1, int y1)
{
  if (got->x0 == x0 && got->y0 == y0 && got->x1 == x1 && got->y1 == y1)
    return 1;

  printf("Failure: box <%d,%d-%d,%d>, want <%d,%d-%d,%d>\n",
         got->x0, got->y0, got->x1, got->y1, x0, y0, x1, y1);
  return 0;
}

result_t layout_test(const char *resources)
{
  static const box_t dims = { 0, 0, 8, 8 };
  static const box_t wide = { 0, 0, 64, 8 };
  static const layout_element_t elements[] =
  {
    { layout_BOX, { .box = { 2, 2, 2 } } },
    { layout_BOX, { .box = { 2, 2, 2 } } },
    { layout_BOX, { .box = { 1, INT_MAX, 2 } } },
  };
  static const layout_element_t newlines[] =
  {
    { layout_NEWLINE, { .box = { 0, 0, 0 } } },
    { layout_BOX,     { .box = { 2, 2, 2 } } },
    { layout_NEWLINE, { .box = { 0, 0, 0 } } },
    { layout_BOX,     { .box = { 3, 3, 2 } } },
  };
  static const layout_element_t tall[] =
  {
    { layout_BOX, { .box = { 2, 2, 20 } } },
  };

  result_t         err;
  box_t            boxes[40];
  layout_element_t many[40];
  int              i;
  txtscr_t        *scr;

  NOT_USED(resources);

  printf("test: boxes share a line until spacing runs out\n");

  /* two fixed boxes and their spacing leave 2, too few for the third with
   * its spacing, so it takes the whole of the next line */
  err = run(&dims, 2, elements, NELEMS(elements), boxes, NELEMS(elements));
  if (err)
    goto failure;
  if (!same_box(&boxes[0], 0, 6, 2, 8) ||
      !same_box(&boxes[1], 4, 6, 6, 8) ||
      !same_box(&boxes[2], 0, 2, 8, 4))
    goto failure;

  scr = txtscr_create(8, 8);
  if (scr == NULL)
    goto failure;
  txtscr_clear(scr);
  for (i = 0; i < (int) NELEMS(elements); i++)
    txtscr_addbox(scr, &boxes[i]);
  txtscr_print(scr);
  txtscr_destroy(scr);

  printf("test: too few boxes to return\n");

  if (run(&dims, 2, elements, NELEMS(elements), boxes, 2) !=
      result_LAYOUT_BUFFER_FULL)
    goto failure;

  printf("test: newlines start lines below the leading\n");

  err = run(&dims, 2, newlines, NELEMS(newlines), boxes, NELEMS(boxes));
  if (err)
    goto failure;
  if (!same_box(&boxes[0], 0, 4, 2, 6) ||
      !same_box(&boxes[1], 0, 0, 3, 2))
    goto failure;

  printf("test: more boxes than a line records wrap\n");

  /* 32 boxes with spacing fill 63 of 64; the other 8 wrap */
  for (i = 0; i < (int) NELEMS(many); i++)
  {
    many[i].type               = layout_BOX;
    many[i].data.box.min_width = 1;
    many[i].data.box.max_width = 1;
    many[i].data.box.height    = 1;
  }
  err = run(&wide, 1, many, NELEMS(many), boxes, NELEMS(boxes));
  if (err)
    goto failure;
  if (!same_box(&boxes[31], 62, 7, 63, 8) ||
      !same_box(&boxes[32],  0, 5,  1, 6) ||
      !same_box(&boxes[39], 14, 5, 15, 6))
    goto failure;

  printf("test: a box that does not fit stops the layout\n");

  if (run(&dims, 2, tall, NELEMS(tall), boxes, NELEMS(boxes)) != result_OK)
    goto failure;

  return result_TEST_PASSED;


failure:

  return result_TEST_FAILED;
}
