/* screen-test.c -- test screen drawing */

#include <stdio.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "geom/box.h"
#include "utils/fxp.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

#define WIDTH  64
#define HEIGHT 64

#define BACKGROUND 0xFF000000

typedef struct testscreen
{
  screen_t            scr;
  pixelfmt_bgrx8888_t pixels[WIDTH * HEIGHT];
}
testscreen_t;

typedef enum linekind
{
  linekind_INT,
  linekind_WU_FIX8,
  linekind_WU_FLOAT
}
linekind_t;

typedef struct linetest
{
  int x0, y0, x1, y1;
}
linetest_t;

/* Rectangles which, taken together, cover the whole canvas without overlap.
 * Three separate partitions: vertical strips, horizontal strips, and an
 * irregular split, mirroring the pieces wuss__clip_to_visible() generates. */
typedef struct partition
{
  int    nboxes;
  box_t  boxes[6];
}
partition_t;

static const partition_t partitions[] =
{
  { 3, { {  0,  0, 20, 64 }, { 20,  0, 41, 64 }, { 41,  0, 64, 64 } } },
  { 3, { {  0,  0, 64, 13 }, {  0, 13, 64, 47 }, {  0, 47, 64, 64 } } },
  { 5, { {  0,  0, 64, 17 }, {  0, 17,  9, 64 }, {  9, 17, 33, 40 },
         { 33, 17, 64, 40 }, {  9, 40, 64, 64 } } }
};

static const linetest_t lines[] =
{
  {  4, 32, 60, 32 }, /* horizontal */
  { 32,  4, 32, 60 }, /* vertical */
  {  4,  4, 60, 60 }, /* 45 degrees */
  { 60, 60,  4,  4 }, /* 45 degrees, reversed */
  {  2, 10, 62, 30 }, /* shallow */
  { 62, 30,  2, 10 }, /* shallow, reversed */
  { 10,  2, 30, 62 }, /* steep */
  { 30, 62, 10,  2 }, /* steep, reversed */
  {  4, 13, 60, 13 }, /* lands on a partition boundary */
  { 20,  0, 20, 64 }, /* lands on a partition boundary */
  { -30, 20, 90, 44 }, /* partially outside */
  { 20, -40, 44, 100 }, /* partially outside */
  { -20, -20, -5, -5 }  /* wholly outside */
};

/* ----------------------------------------------------------------------- */

static void testscreen_init(testscreen_t *ts)
{
  int i;

  for (i = 0; i < WIDTH * HEIGHT; i++)
    ts->pixels[i] = BACKGROUND;

  screen_init(&ts->scr, SIZE2D(WIDTH, HEIGHT),
              pixelfmt_bgrx8888,
              WIDTH * (int) sizeof(ts->pixels[0]),
              NULL,
              ts->pixels);
}

static void draw(screen_t         *scr,
                 linekind_t        kind,
                 const linetest_t *line,
                 colour_t          colour)
{
  switch (kind)
  {
  case linekind_INT:
    screen_draw_line(scr, line->x0, line->y0, line->x1, line->y1, colour);
    break;

  case linekind_WU_FIX8:
    /* multiply, not INT_TO_FIX8: coords may be negative (left shift is UB). */
    screen_draw_line_wu_fix8(scr,
                             line->x0 * FIX8_ONE, line->y0 * FIX8_ONE,
                             line->x1 * FIX8_ONE, line->y1 * FIX8_ONE,
                             colour);
    break;

  case linekind_WU_FLOAT:
    screen_draw_line_wu_float(scr,
                              (float) line->x0, (float) line->y0,
                              (float) line->x1, (float) line->y1,
                              colour);
    break;
  }
}

/* ----------------------------------------------------------------------- */

/* The same logical line drawn in one go, and drawn once per piece of a
 * partition of the canvas, must produce identical pixels. */
static result_t test_clip_invariance(void)
{
  static testscreen_t reference;
  static testscreen_t pieced;

  colour_t colour;
  size_t   l, p, k;
  int      b;

  colour = colour_rgb(255, 255, 255);

  for (k = 0; k < 3; k++)
    for (l = 0; l < NELEMS(lines); l++)
    {
      testscreen_init(&reference);
      draw(&reference.scr, (linekind_t) k, &lines[l], colour);

      for (p = 0; p < NELEMS(partitions); p++)
      {
        testscreen_init(&pieced);

        for (b = 0; b < partitions[p].nboxes; b++)
        {
          pieced.scr.clip = partitions[p].boxes[b];
          draw(&pieced.scr, (linekind_t) k, &lines[l], colour);
        }

        if (memcmp(reference.pixels, pieced.pixels, sizeof(reference.pixels)))
        {
          printf("screen: clip invariance failed for line %zu, "
                 "rasterizer %zu, partition %zu\n", l, k, p);
          return result_TEST_FAILED;
        }
      }
    }

  return result_TEST_PASSED;
}

/* Guard against the test above passing vacuously because clipping stopped
 * happening at all: pixels outside the clip box must stay untouched. */
static result_t test_clipping_still_happens(void)
{
  static const box_t clip = { 0, 0, 64, 20 };

  static testscreen_t ts;

  colour_t colour;
  size_t   k;
  int      x, y;

  colour = colour_rgb(255, 255, 255);

  for (k = 0; k < 3; k++)
  {
    const linetest_t line = { 4, 4, 60, 60 }; /* crosses the clip boundary */

    testscreen_init(&ts);
    ts.scr.clip = clip;
    draw(&ts.scr, (linekind_t) k, &line, colour);

    for (y = 0; y < HEIGHT; y++)
      for (x = 0; x < WIDTH; x++)
      {
        if (box_contains_point(&clip, x, y))
          continue;

        if (ts.pixels[y * WIDTH + x] != BACKGROUND)
        {
          printf("screen: pixel (%d,%d) drawn outside the clip box "
                 "by rasterizer %zu\n", x, y, k);
          return result_TEST_FAILED;
        }
      }
  }

  return result_TEST_PASSED;
}

/* ----------------------------------------------------------------------- */

/* Wu fix8 lines with large and off-screen endpoints must not trip
 * UndefinedBehaviorSanitizer: the gradient maths once overflowed 32-bit int
 * (FIX16_ONE * dy_f8) and left-shifted negative pixel coordinates. */
static result_t test_wu_fix8_extreme_coords(void)
{
  /* fix8: value * 256, written out to avoid left-shifting negatives here too. */
  static const fix8_t endpoints[][4] =
  {
    {  -1000 * 256,     32 * 256,   2000 * 256,     33 * 256 },
    {     32 * 256,  -1000 * 256,     31 * 256,   2000 * 256 },
    {  -5000 * 256,  -5000 * 256,   5000 * 256,   5000 * 256 },
    { -32000 * 256,     10 * 256,  32000 * 256,     50 * 256 }
  };

  static testscreen_t ts;

  colour_t colour;
  size_t   i;

  colour = colour_rgb(255, 255, 255);

  for (i = 0; i < NELEMS(endpoints); i++)
  {
    testscreen_init(&ts);
    screen_draw_line_wu_fix8(&ts.scr,
                             endpoints[i][0], endpoints[i][1],
                             endpoints[i][2], endpoints[i][3],
                             colour);
  }

  return result_TEST_PASSED;
}

/* ----------------------------------------------------------------------- */

result_t screen_test(const char *resources)
{
  typedef result_t (*screentestfn)(void);

  static const screentestfn tests[] =
  {
    test_clip_invariance,
    test_clipping_still_happens,
    test_wu_fix8_extreme_coords
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
