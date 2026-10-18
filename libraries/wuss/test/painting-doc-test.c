/* wuss/test/painting-doc-test.c -- paint canvas drawing and undo tests */

#include <stdint.h>
#include <stdio.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "geom/box.h"
#include "geom/point.h"
#include "geom/size.h"

#include "../tasks/painting-doc.h"

#include "test/all-tests.h"
#include "test/tmpdir.h"

/* ----------------------------------------------------------------------- */

#define WHITE 0x00FFFFFFu
#define BLACK 0x00000000u
#define RED   0x00FF0000u

#define CHECK(cond) \
  do \
  { \
    if (!(cond)) \
    { \
      printf("paintingdoc: line %d: %s\n", __LINE__, #cond); \
      paintingdoc_destroy(doc); \
      return result_TEST_FAILED; \
    } \
  } \
  while (0)

static uint32_t px(const paintingdoc_t *doc, int x, int y)
{
  const bitmap_t *bm;

  bm = paintingdoc_bitmap(doc);

  return ((const uint32_t *) ((const uint8_t *) bm->base +
                              (size_t) y * bm->rowbytes))[x] & 0x00FFFFFFu;
}

static colour_t black(void)
{
  return colour_rgb(0, 0, 0);
}

static colour_t red(void)
{
  return colour_rgb(255, 0, 0);
}

/* ----------------------------------------------------------------------- */

result_t paintingdoc_test(const char *resources)
{
  paintingdoc_t *doc = NULL;
  box_t          dirty;
  char           path[256];
  int            i, n, x, y;
  uint32_t       before, after;

  printf("test: create makes a white bgrx8888 canvas; a bad size is "
         "rejected\n");

  CHECK(paintingdoc_create(SIZE2D(0, 10), &doc) == result_BAD_ARG);
  CHECK(paintingdoc_create(SIZE2D(10, -1), &doc) == result_BAD_ARG);
  CHECK(paintingdoc_create(SIZE2D(20, 10), &doc) == result_OK);
  CHECK(paintingdoc_bitmap(doc)->size.w == 20);
  CHECK(paintingdoc_bitmap(doc)->size.h == 10);
  CHECK(paintingdoc_bitmap(doc)->format == pixelfmt_bgrx8888);
  CHECK(px(doc, 0, 0) == WHITE);
  CHECK(px(doc, 19, 9) == WHITE);
  CHECK(!paintingdoc_can_undo(doc));
  paintingdoc_destroy(doc);

  printf("test: create_filled fills the canvas with the colour\n");

  CHECK(paintingdoc_create_filled(SIZE2D(0, 10), red(), &doc) == result_BAD_ARG);
  CHECK(paintingdoc_create_filled(SIZE2D(4, 3), red(), &doc) == result_OK);
  CHECK(px(doc, 0, 0) == RED);
  CHECK(px(doc, 3, 2) == RED);
  paintingdoc_destroy(doc);
  CHECK(paintingdoc_create(SIZE2D(20, 10), &doc) == result_OK);

  printf("test: a width 1 stroke sets just its pixels and reports its box\n");

  dirty = paintingdoc_stroke(doc, POINT(2, 5), POINT(8, 5), 1, black());
  CHECK(px(doc, 2, 5) == BLACK);
  CHECK(px(doc, 5, 5) == BLACK);
  CHECK(px(doc, 8, 5) == BLACK);
  CHECK(px(doc, 5, 4) == WHITE);
  CHECK(px(doc, 5, 6) == WHITE);
  CHECK(px(doc, 9, 5) == WHITE);
  CHECK(dirty.x0 <= 2 && dirty.x1 >= 9 && dirty.y0 <= 5 && dirty.y1 >= 6);

  printf("test: a wide dab is a round brush; one off the canvas is clipped, "
         "not fatal\n");

  dirty = paintingdoc_stroke(doc, POINT(10, 3), POINT(10, 3), 5, red());
  CHECK(px(doc, 10, 3) == RED);
  CHECK(px(doc, 12, 3) == RED);
  CHECK(px(doc, 8, 3) == RED);
  CHECK(px(doc, 13, 3) == WHITE);
  CHECK(px(doc, 12, 1) == WHITE); /* corner of the 5x5 square: not round */

  dirty = paintingdoc_stroke(doc, POINT(-30, -30), POINT(-20, -25), 3, red());
  CHECK(box_is_empty(&dirty));

  dirty = paintingdoc_stroke(doc, POINT(-2, 0), POINT(1, 0), 1, red());
  CHECK(px(doc, 0, 0) == RED);
  CHECK(px(doc, 1, 0) == RED);
  CHECK(dirty.x0 >= 0 && dirty.y0 >= 0);

  paintingdoc_destroy(doc);
  doc = NULL;

  printf("test: rect fills, outlines inside its corners in either order, and "
         "clips\n");

  CHECK(paintingdoc_create(SIZE2D(20, 20), &doc) == result_OK);

  dirty = paintingdoc_rect(doc, POINT(6, 6), POINT(2, 2), 1, 0, black());
  CHECK(px(doc, 2, 2) == BLACK);
  CHECK(px(doc, 6, 6) == BLACK);
  CHECK(px(doc, 6, 2) == BLACK);
  CHECK(px(doc, 4, 4) == WHITE);
  CHECK(px(doc, 7, 7) == WHITE);
  CHECK(px(doc, 1, 4) == WHITE);
  CHECK(dirty.x0 == 2 && dirty.y0 == 2 && dirty.x1 == 7 && dirty.y1 == 7);

  paintingdoc_rect(doc, POINT(10, 10), POINT(14, 14), 1, 1, red());
  CHECK(px(doc, 12, 12) == RED);
  CHECK(px(doc, 10, 10) == RED);
  CHECK(px(doc, 14, 14) == RED);
  CHECK(px(doc, 15, 14) == WHITE);

  paintingdoc_rect(doc, POINT(15, 15), POINT(30, 30), 1, 1, black());
  CHECK(px(doc, 19, 19) == BLACK);

  dirty = paintingdoc_rect(doc, POINT(40, 40), POINT(50, 50), 1, 1, black());
  CHECK(box_is_empty(&dirty));

  printf("test: a thick outline too wide for the rect fills it\n");

  paintingdoc_rect(doc, POINT(0, 14), POINT(4, 18), 3, 0, red());
  CHECK(px(doc, 2, 16) == RED);

  paintingdoc_destroy(doc);
  doc = NULL;

  printf("test: flood fill stops at an outline, fills exactly the region, and "
         "reports its box\n");

  CHECK(paintingdoc_create(SIZE2D(20, 20), &doc) == result_OK);
  paintingdoc_rect(doc, POINT(5, 5), POINT(14, 14), 1, 0, black());

  dirty = paintingdoc_fill(doc, POINT(8, 8), red());
  CHECK(px(doc, 8, 8) == RED);
  CHECK(px(doc, 6, 6) == RED);
  CHECK(px(doc, 13, 13) == RED);
  CHECK(px(doc, 5, 8) == BLACK);
  CHECK(px(doc, 2, 2) == WHITE);
  CHECK(px(doc, 19, 19) == WHITE);
  CHECK(dirty.x0 == 6 && dirty.y0 == 6 && dirty.x1 == 14 && dirty.y1 == 14);

  printf("test: flood fill is 4-connected: a one pixel diagonal line is a "
         "wall\n");

  paintingdoc_destroy(doc);
  doc = NULL;
  CHECK(paintingdoc_create(SIZE2D(10, 10), &doc) == result_OK);
  paintingdoc_stroke(doc, POINT(0, 0), POINT(9, 9), 1, black());

  paintingdoc_fill(doc, POINT(5, 0), red());
  CHECK(px(doc, 5, 0) == RED);
  CHECK(px(doc, 9, 8) == RED);
  CHECK(px(doc, 0, 5) == WHITE);
  CHECK(px(doc, 8, 9) == WHITE);
  CHECK(px(doc, 4, 4) == BLACK);

  printf("test: filling with the colour already there, or off the canvas, "
         "changes nothing and takes no undo step\n");

  dirty = paintingdoc_fill(doc, POINT(5, 0), red());
  CHECK(box_is_empty(&dirty));
  dirty = paintingdoc_fill(doc, POINT(-1, 3), red());
  CHECK(box_is_empty(&dirty));
  dirty = paintingdoc_fill(doc, POINT(3, 99), red());
  CHECK(box_is_empty(&dirty));

  /* only the one real fill above left a step */
  n = 0;
  while (paintingdoc_can_undo(doc))
  {
    paintingdoc_undo(doc);
    n++;
  }
  CHECK(n == 1);

  paintingdoc_destroy(doc);
  doc = NULL;

  printf("test: flood fill of a large blank canvas fills every pixel\n");

  CHECK(paintingdoc_create(SIZE2D(640, 480), &doc) == result_OK);
  dirty = paintingdoc_fill(doc, POINT(320, 240), red());
  CHECK(dirty.x0 == 0 && dirty.y0 == 0 && dirty.x1 == 640 && dirty.y1 == 480);
  n = 0;
  for (y = 0; y < 480; y++)
    for (x = 0; x < 640; x++)
      n += px(doc, x, y) == RED;
  CHECK(n == 640 * 480);

  printf("test: flood fill covers a winding region (a serpentine corridor)\n");

  paintingdoc_destroy(doc);
  doc = NULL;
  CHECK(paintingdoc_create(SIZE2D(41, 41), &doc) == result_OK);

  /* walls on every other row, each with a gap alternating right and left */
  for (y = 1; y < 41; y += 2)
  {
    paintingdoc_stroke(doc, POINT(0, y), POINT(40, y), 1, black());
    x = (y / 2) % 2 ? 0 : 40;
    paintingdoc_stroke(doc, POINT(x, y), POINT(x, y), 1,
                    colour_rgb(255, 255, 255));
  }

  paintingdoc_fill(doc, POINT(20, 0), red());
  CHECK(px(doc, 20, 40) == RED); /* reached the far end of the corridor */
  CHECK(px(doc, 20, 1) == BLACK);

  paintingdoc_destroy(doc);
  doc = NULL;

  printf("test: undo restores the canvas to the last checkpoint, newest "
         "first\n");

  CHECK(paintingdoc_create(SIZE2D(20, 20), &doc) == result_OK);
  CHECK(paintingdoc_undo(doc).x1 <= 0 && !paintingdoc_can_undo(doc));

  CHECK(paintingdoc_checkpoint(doc) == result_OK);
  paintingdoc_stroke(doc, POINT(1, 1), POINT(10, 1), 1, black());
  CHECK(paintingdoc_checkpoint(doc) == result_OK);
  paintingdoc_stroke(doc, POINT(1, 3), POINT(10, 3), 1, red());
  CHECK(paintingdoc_can_undo(doc));

  dirty = paintingdoc_undo(doc);
  CHECK(dirty.x0 == 0 && dirty.y0 == 0 && dirty.x1 == 20 && dirty.y1 == 20);
  CHECK(px(doc, 5, 1) == BLACK);
  CHECK(px(doc, 5, 3) == WHITE);

  paintingdoc_undo(doc);
  CHECK(px(doc, 5, 1) == WHITE);
  CHECK(!paintingdoc_can_undo(doc));

  printf("test: rect and fill each take their own undo step\n");

  paintingdoc_rect(doc, POINT(2, 2), POINT(8, 8), 1, 1, black());
  paintingdoc_fill(doc, POINT(15, 15), red());
  CHECK(px(doc, 15, 15) == RED);
  paintingdoc_undo(doc);
  CHECK(px(doc, 15, 15) == WHITE);
  CHECK(px(doc, 5, 5) == BLACK);
  paintingdoc_undo(doc);
  CHECK(px(doc, 5, 5) == WHITE);
  CHECK(!paintingdoc_can_undo(doc));

  printf("test: undo keeps at most PAINTINGDOC_UNDO_MAX steps, dropping the "
         "oldest\n");

  for (i = 0; i < PAINTINGDOC_UNDO_MAX + 5; i++)
  {
    CHECK(paintingdoc_checkpoint(doc) == result_OK);
    paintingdoc_stroke(doc, POINT(i % 20, 0), POINT(i % 20, 0), 1, black());
  }

  n = 0;
  while (paintingdoc_can_undo(doc))
  {
    paintingdoc_undo(doc);
    n++;
  }
  CHECK(n == PAINTINGDOC_UNDO_MAX);
  /* the five oldest steps were dropped: their strokes cannot be undone */
  CHECK(px(doc, 0, 0) == BLACK);
  CHECK(px(doc, 4, 0) == BLACK);
  CHECK(px(doc, 5, 0) == WHITE);

  printf("test: loading a paletted or an RGB PNG resizes the canvas, drops "
         "the undo history and keeps the picture\n");

  snprintf(path, sizeof(path), "%s/resources/wuss-tasks/images/bob-lady.png",
           resources);
  CHECK(paintingdoc_checkpoint(doc) == result_OK);
  CHECK(paintingdoc_load_png(doc, path) == result_OK);
  CHECK(paintingdoc_bitmap(doc)->size.w == 64);
  CHECK(paintingdoc_bitmap(doc)->size.h == 64);
  CHECK(!paintingdoc_can_undo(doc));

  /* a sprite is not a blank white sheet */
  n = 0;
  for (y = 0; y < 64; y++)
    for (x = 0; x < 64; x++)
      n += px(doc, x, y) != WHITE;
  CHECK(n > 100);

  snprintf(path, sizeof(path), "%s/resources/wuss-tasks/images/smred.png",
           resources);
  CHECK(paintingdoc_load_png(doc, path) == result_OK);
  CHECK(paintingdoc_bitmap(doc)->size.w == 512);
  CHECK(paintingdoc_bitmap(doc)->size.h == 512);

  printf("test: a PNG that will not load leaves the canvas alone\n");

  paintingdoc_stroke(doc, POINT(5, 5), POINT(5, 5), 1, red());
  snprintf(path, sizeof(path),
           "%s/resources/wuss-tasks/images/no-such-file.png", resources);
  CHECK(paintingdoc_load_png(doc, path) != result_OK);
  CHECK(paintingdoc_bitmap(doc)->size.w == 512);
  CHECK(px(doc, 5, 5) == RED);

  printf("test: a saved canvas loads back the same\n");

  CHECK(test_tmppath(path, sizeof(path), "dptlib-paintingdoc-test.png"));
  before = px(doc, 6, 5);
  after  = px(doc, 300, 400);
  CHECK(bitmap_save_png(paintingdoc_bitmap(doc), path) == result_OK);
  paintingdoc_destroy(doc);

  CHECK(paintingdoc_create(SIZE2D(8, 8), &doc) == result_OK);
  CHECK(paintingdoc_load_png(doc, path) == result_OK);
  remove(path);
  CHECK(paintingdoc_bitmap(doc)->size.w == 512);
  CHECK(px(doc, 5, 5) == RED);
  CHECK(px(doc, 6, 5) == before);
  CHECK(px(doc, 300, 400) == after);

  paintingdoc_destroy(doc);

  printf("paintingdoc: all tests passed\n");

  return result_TEST_PASSED;
}
