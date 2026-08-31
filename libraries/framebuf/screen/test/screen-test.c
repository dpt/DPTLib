/* screen-test.c -- test screen drawing */

#include <stdio.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
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

/* 9x9 source: each 3x3 cell a distinct solid colour, indexed [row][col]. */
#define NP_SRC 9
#define NP_CELL 3

static const int np_rgb[3][3][3] =
{
  { { 255,   0,   0 }, { 255, 255,   0 }, {   0, 255,   0 } },
  { {   0, 255, 255 }, { 128, 128, 128 }, {   0,   0, 255 } },
  { { 255,   0, 255 }, { 255, 255, 255 }, {  64,  64,  64 } }
};

/* Encode an rgb colour to a screen pixel the same way the draw path does. */
static pixelfmt_bgrx8888_t np_encode(testscreen_t *ts, int r, int g, int b)
{
  testscreen_init(ts);
  screen_draw_pixel(&ts->scr, 0, 0, colour_rgb(r, g, b));
  return ts->pixels[0];
}

static void np_make_src(bitmap_t *src, pixelfmt_rgba8888_t *buf)
{
  int cx, cy, x, y;

  for (cy = 0; cy < 3; cy++)
    for (cx = 0; cx < 3; cx++)
    {
      colour_t c;

      c = colour_rgb(np_rgb[cy][cx][0], np_rgb[cy][cx][1], np_rgb[cy][cx][2]);

      for (y = 0; y < NP_CELL; y++)
        for (x = 0; x < NP_CELL; x++)
          buf[(cy * NP_CELL + y) * NP_SRC + (cx * NP_CELL + x)] = c.primary;
    }

  bitmap_init(src, SIZE2D(NP_SRC, NP_SRC), pixelfmt_rgba8888,
              NP_SRC * (int) sizeof(buf[0]), NULL, buf);
}

static int np_at(testscreen_t *ts, int x, int y)
{
  return (int) ts->pixels[y * WIDTH + x];
}

static result_t test_ninepatch(void)
{
  static testscreen_t ts;
  static testscreen_t enc;
  static pixelfmt_rgba8888_t srcbuf[NP_SRC * NP_SRC];

  bitmap_t src;
  box_t    dst = { 5, 5, 45, 45 };
  int      exp[3][3];
  int      cx, cy;

  np_make_src(&src, srcbuf);

  for (cy = 0; cy < 3; cy++)
    for (cx = 0; cx < 3; cx++)
      exp[cy][cx] = (int) np_encode(&enc,
                                    np_rgb[cy][cx][0],
                                    np_rgb[cy][cx][1],
                                    np_rgb[cy][cx][2]);

  /* Normal case. */
  testscreen_init(&ts);
  screen_draw_ninepatch(&ts.scr, &dst, &src, 0);

  /* Corners: the 3x3 block at each destination corner is that corner colour. */
  if (np_at(&ts, 5, 5)     != exp[0][0] || np_at(&ts, 7, 7)   != exp[0][0] ||
      np_at(&ts, 44, 5)    != exp[0][2] || np_at(&ts, 42, 7)   != exp[0][2] ||
      np_at(&ts, 5, 44)    != exp[2][0] || np_at(&ts, 7, 42)   != exp[2][0] ||
      np_at(&ts, 44, 44)   != exp[2][2] || np_at(&ts, 42, 42)  != exp[2][2])
  {
    printf("screen: ninepatch corner mismatch\n");
    return result_TEST_FAILED;
  }

  /* Mid-edge and interior. */
  if (np_at(&ts, 25, 6)  != exp[0][1] || /* top edge */
      np_at(&ts, 25, 43) != exp[2][1] || /* bottom edge */
      np_at(&ts, 6, 25)  != exp[1][0] || /* left edge */
      np_at(&ts, 43, 25) != exp[1][2] || /* right edge */
      np_at(&ts, 25, 25) != exp[1][1])   /* centre */
  {
    printf("screen: ninepatch edge/centre mismatch\n");
    return result_TEST_FAILED;
  }

  /* Clipping: a pixel just outside dst stays background. */
  if (np_at(&ts, 4, 4) != (int) (pixelfmt_bgrx8888_t) BACKGROUND ||
      np_at(&ts, 45, 45) != (int) (pixelfmt_bgrx8888_t) BACKGROUND)
  {
    printf("screen: ninepatch drew outside dst\n");
    return result_TEST_FAILED;
  }

  /* Clip composition: restrict to the left half, the right half is untouched. */
  testscreen_init(&ts);
  ts.scr.clip = (box_t) { 0, 0, 25, 64 };
  screen_draw_ninepatch(&ts.scr, &dst, &src, 0);
  if (np_at(&ts, 6, 25) != exp[1][0] ||
      np_at(&ts, 30, 25) != (int) (pixelfmt_bgrx8888_t) BACKGROUND)
  {
    printf("screen: ninepatch ignored the screen clip\n");
    return result_TEST_FAILED;
  }
  /* The clip is restored on return. */
  if (!box_is_empty(&ts.scr.clip) &&
      (ts.scr.clip.x0 != 0 || ts.scr.clip.x1 != 25))
  {
    printf("screen: ninepatch did not restore the clip\n");
    return result_TEST_FAILED;
  }

  /* Degenerate: dst exactly two cells each way -> only corners, no centre. */
  testscreen_init(&ts);
  {
    box_t small = { 10, 10, 10 + 2 * NP_CELL, 10 + 2 * NP_CELL };

    screen_draw_ninepatch(&ts.scr, &small, &src, 0);
    if (np_at(&ts, 10, 10) != exp[0][0] ||
        np_at(&ts, 15, 15) != exp[2][2] ||
        np_at(&ts, 12, 12) == exp[1][1]) /* centre colour must NOT appear */
    {
      printf("screen: ninepatch degenerate case wrong\n");
      return result_TEST_FAILED;
    }
  }

  /* NO_CENTRE: border drawn, interior stays background. */
  testscreen_init(&ts);
  screen_draw_ninepatch(&ts.scr, &dst, &src, screen_NINEPATCH_NO_CENTRE);
  if (np_at(&ts, 5, 5)   != exp[0][0] || /* corner still drawn */
      np_at(&ts, 25, 6)  != exp[0][1] || /* edge still drawn */
      np_at(&ts, 25, 25) != (int) (pixelfmt_bgrx8888_t) BACKGROUND) /* centre skipped */
  {
    printf("screen: ninepatch NO_CENTRE wrong\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

/* ----------------------------------------------------------------------- */

static result_t test_fill_pattern(void)
{
  static testscreen_t ts;
  static testscreen_t enc;

  box_t    box = { 8, 8, 24, 24 };
  int      fg, bg;

  fg = (int) np_encode(&enc, 255, 0, 0);
  bg = (int) np_encode(&enc, 0, 0, 255);

  /* GREY50 is 0xAA,0x55,... : at origin (0,0) pixel (x,y) is fg when
   * ((x ^ y) & 1) == 0. */
  testscreen_init(&ts);
  screen_fill_pattern(&ts.scr, &box, screen_PATTERN_GREY50, 0, 0,
                      colour_rgb(255, 0, 0), colour_rgb(0, 0, 255));

  if (np_at(&ts, 8, 8)  != fg ||  /* (0,0) phase -> set bit */
      np_at(&ts, 9, 8)  != bg ||
      np_at(&ts, 8, 9)  != bg ||
      np_at(&ts, 9, 9)  != fg)
  {
    printf("screen: fill_pattern GREY50 wrong at origin 0\n");
    return result_TEST_FAILED;
  }

  /* Outside the box stays background. */
  if (np_at(&ts, 7, 7)  != (int) (pixelfmt_bgrx8888_t) BACKGROUND ||
      np_at(&ts, 24, 24) != (int) (pixelfmt_bgrx8888_t) BACKGROUND)
  {
    printf("screen: fill_pattern drew outside the box\n");
    return result_TEST_FAILED;
  }

  /* Shift the origin by one in x: every pixel's phase flips, so the same
   * screen coordinate takes the other colour. */
  testscreen_init(&ts);
  screen_fill_pattern(&ts.scr, &box, screen_PATTERN_GREY50, 1, 0,
                      colour_rgb(255, 0, 0), colour_rgb(0, 0, 255));
  if (np_at(&ts, 8, 8) != bg || np_at(&ts, 9, 8) != fg)
  {
    printf("screen: fill_pattern ignored origin phase\n");
    return result_TEST_FAILED;
  }

  /* Honours the screen clip. */
  testscreen_init(&ts);
  ts.scr.clip = (box_t) { 0, 0, 16, 64 };
  screen_fill_pattern(&ts.scr, &box, screen_PATTERN_SOLID, 0, 0,
                      colour_rgb(255, 0, 0), colour_rgb(0, 0, 255));
  if (np_at(&ts, 10, 10) != fg ||
      np_at(&ts, 20, 10) != (int) (pixelfmt_bgrx8888_t) BACKGROUND)
  {
    printf("screen: fill_pattern ignored the screen clip\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

/* ----------------------------------------------------------------------- */

static result_t test_dashed_line(void)
{
  static testscreen_t ts;
  static testscreen_t enc;

  int fg;
  int x;
  int on_count, off_count;

  fg = (int) np_encode(&enc, 255, 0, 0);

  /* Horizontal line y=10, x in [0,19], on=2 off=2: phase cycles
   * 0,1 (drawn) 2,3 (skipped) starting at x=0. */
  testscreen_init(&ts);
  screen_draw_dashed_line(&ts.scr, 0, 10, 19, 10, 2, 2, colour_rgb(255, 0, 0));

  on_count = off_count = 0;
  for (x = 0; x <= 19; x++)
  {
    int lit = (np_at(&ts, x, 10) == fg);
    int want = ((x % 4) < 2);
    if (lit != want)
    {
      printf("screen: dashed_line wrong at x=%d (lit=%d want=%d)\n",
             x, lit, want);
      return result_TEST_FAILED;
    }
    if (lit) on_count++; else off_count++;
  }
  if (on_count != 10 || off_count != 10)
  {
    printf("screen: dashed_line dash ratio off (on=%d off=%d)\n",
           on_count, off_count);
    return result_TEST_FAILED;
  }

  /* off <= 0 gives a solid line. */
  testscreen_init(&ts);
  screen_draw_dashed_line(&ts.scr, 0, 5, 9, 5, 3, 0, colour_rgb(255, 0, 0));
  for (x = 0; x <= 9; x++)
  {
    if (np_at(&ts, x, 5) != fg)
    {
      printf("screen: dashed_line with off=0 left a gap at x=%d\n", x);
      return result_TEST_FAILED;
    }
  }

  /* on <= 0 draws nothing. */
  testscreen_init(&ts);
  screen_draw_dashed_line(&ts.scr, 0, 7, 9, 7, 0, 4, colour_rgb(255, 0, 0));
  for (x = 0; x <= 9; x++)
  {
    if (np_at(&ts, x, 7) != (int) (pixelfmt_bgrx8888_t) BACKGROUND)
    {
      printf("screen: dashed_line with on=0 drew a pixel at x=%d\n", x);
      return result_TEST_FAILED;
    }
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
    test_wu_fix8_extreme_coords,
    test_ninepatch,
    test_fill_pattern,
    test_dashed_line
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
