/* framebuf/screen/test/screen-test.c -- test screen drawing */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/screen.h"
#include "framebuf/span-p4.h"
#include "geom/box.h"
#include "geom/point.h"
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
 * irregular split, as a window manager's visible-region clipping generates. */
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

  colour_t            colour;
  size_t              l, p, k;
  int                 b;

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

  colour_t            colour;
  size_t              k;
  int                 x, y;

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

  colour_t            colour;
  size_t              i;

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
  screen_set_pixel(&ts->scr, 0, 0, colour_rgb(r, g, b));
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

static result_t test_tiled(void)
{
  static testscreen_t        ts;
  static pixelfmt_bgrx8888_t srcbuf[3 * 2];

  bitmap_t                   src;
  box_t                      dst = { 5, 6, 30, 25 };
  box_t                      saved;
  point_t                    origin;
  int                        x, y;
  int                        want;

  for (x = 0; x < 3 * 2; x++)
    srcbuf[x] = 0x00000100 + x; /* each tile pixel is unique */
  bitmap_init(&src, SIZE2D(3, 2), pixelfmt_bgrx8888,
              3 * (int) sizeof(srcbuf[0]), NULL, srcbuf);

  /* Origin well off-screen to the top left, then inside dst, then below and
   * right of it: every pixel in dst must show the tile pixel its phase
   * says, and nothing outside dst may change. */
  for (origin = POINT(-100, -50); ; origin = POINT(origin.x + 107, origin.y + 61))
  {
    testscreen_init(&ts);
    saved = ts.scr.clip;
    if (screen_copy_bitmap_tiled(&ts.scr, &dst, &src, origin) != result_OK)
    {
      printf("screen: tiled copy failed\n");
      return result_TEST_FAILED;
    }

    for (y = 0; y < HEIGHT; y++)
      for (x = 0; x < WIDTH; x++)
      {
        if (x >= dst.x0 && x < dst.x1 && y >= dst.y0 && y < dst.y1)
        {
          want = 0x00000100 + (((y - origin.y) % 2 + 2) % 2) * 3 +
                              (((x - origin.x) % 3 + 3) % 3);
          if ((np_at(&ts, x, y) & 0xFFFFFF) != want)
          {
            printf("screen: tiled phase wrong at (%d,%d), origin (%d,%d)\n",
                   x, y, origin.x, origin.y);
            return result_TEST_FAILED;
          }
        }
        else if (np_at(&ts, x, y) != (int) (pixelfmt_bgrx8888_t) BACKGROUND)
        {
          printf("screen: tiled drew outside dst at (%d,%d)\n", x, y);
          return result_TEST_FAILED;
        }
      }

    if (memcmp(&ts.scr.clip, &saved, sizeof(saved)) != 0)
    {
      printf("screen: tiled did not restore the clip\n");
      return result_TEST_FAILED;
    }

    if (origin.x > 100)
      break;
  }

  return result_TEST_PASSED;
}

static result_t test_ninepatch(void)
{
  static testscreen_t        ts;
  static testscreen_t        enc;
  static pixelfmt_rgba8888_t srcbuf[NP_SRC * NP_SRC];

  bitmap_t                   src;
  box_t                      dst = { 5, 5, 45, 45 };
  int                        exp[3][3];
  int                        cx, cy;

  np_make_src(&src, srcbuf);

  for (cy = 0; cy < 3; cy++)
    for (cx = 0; cx < 3; cx++)
      exp[cy][cx] = (int) np_encode(&enc,
                                    np_rgb[cy][cx][0],
                                    np_rgb[cy][cx][1],
                                    np_rgb[cy][cx][2]);

  /* Normal case. */
  testscreen_init(&ts);
  screen_copy_ninepatch(&ts.scr, &dst, &src, 0);

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
  screen_copy_ninepatch(&ts.scr, &dst, &src, 0);
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

    screen_copy_ninepatch(&ts.scr, &small, &src, 0);
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
  screen_copy_ninepatch(&ts.scr, &dst, &src, screen_NINEPATCH_NO_CENTRE);
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

  box_t box = { 8, 8, 24, 24 };
  int   fg, bg;

  fg = (int) np_encode(&enc, 255, 0, 0);
  bg = (int) np_encode(&enc, 0, 0, 255);

  /* GREY50 is 0xAA,0x55,... : at origin (0,0) pixel (x,y) is fg when
   * ((x ^ y) & 1) == 0. */
  testscreen_init(&ts);
  {
    pattern_t pat = pattern_from_preset(screen_PATTERN_GREY50,
                                        colour_rgb(255, 0, 0),
                                        colour_rgb(0, 0, 255));
    screen_fill_pattern(&ts.scr, &box, &pat);
  }

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
  {
    pattern_t pat = pattern_from_preset(screen_PATTERN_GREY50,
                                        colour_rgb(255, 0, 0),
                                        colour_rgb(0, 0, 255));
    pat.origin = POINT(1, 0);
    screen_fill_pattern(&ts.scr, &box, &pat);
  }
  if (np_at(&ts, 8, 8) != bg || np_at(&ts, 9, 8) != fg)
  {
    printf("screen: fill_pattern ignored origin phase\n");
    return result_TEST_FAILED;
  }

  /* Honours the screen clip. */
  testscreen_init(&ts);
  ts.scr.clip = (box_t) { 0, 0, 16, 64 };
  {
    pattern_t pat = pattern_from_preset(screen_PATTERN_SOLID,
                                        colour_rgb(255, 0, 0),
                                        colour_rgb(0, 0, 255));
    screen_fill_pattern(&ts.scr, &box, &pat);
  }
  if (np_at(&ts, 10, 10) != fg ||
      np_at(&ts, 20, 10) != (int) (pixelfmt_bgrx8888_t) BACKGROUND)
  {
    printf("screen: fill_pattern ignored the screen clip\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

/* ----------------------------------------------------------------------- */

static result_t test_fill_checks(void)
{
  static testscreen_t ts;
  static testscreen_t enc;

  const int back = (int) (pixelfmt_bgrx8888_t) BACKGROUND;

  box_t     box = { 8, 8, 24, 24 };
  colour_t  red, blue;
  int       ev, od;

  red  = colour_rgb(255, 0, 0);
  blue = colour_rgb(0, 0, 255);
  ev   = (int) np_encode(&enc, 255, 0, 0);
  od   = (int) np_encode(&enc, 0, 0, 255);

  /* origin (2,3), 4px squares: (8,8) is square (1,1), even; (10,8) is
   * (2,1) and (8,11) is (1,2), both odd */
  testscreen_init(&ts);
  screen_fill_checks(&ts.scr, &box, POINT(2, 3), 4, red, blue);
  if (np_at(&ts, 8, 8)  != ev ||
      np_at(&ts, 9, 9)  != ev ||
      np_at(&ts, 10, 8) != od ||
      np_at(&ts, 8, 11) != od)
  {
    printf("screen: fill_checks wrong phase\n");
    return result_TEST_FAILED;
  }
  if (np_at(&ts, 7, 7) != back || np_at(&ts, 24, 24) != back)
  {
    printf("screen: fill_checks drew outside the box\n");
    return result_TEST_FAILED;
  }

  /* origin beyond the box, so square indices are negative: (8,8) is square
   * (-3,-3), even; (12,8) is (-2,-3), odd; (16,8) is (-1,-3), even */
  testscreen_init(&ts);
  screen_fill_checks(&ts.scr, &box, POINT(20, 20), 4, red, blue);
  if (np_at(&ts, 8, 8)  != ev ||
      np_at(&ts, 12, 8) != od ||
      np_at(&ts, 16, 8) != ev)
  {
    printf("screen: fill_checks wrong phase left of origin\n");
    return result_TEST_FAILED;
  }

  /* honours the screen clip */
  testscreen_init(&ts);
  ts.scr.clip = (box_t) { 0, 0, 16, 64 };
  screen_fill_checks(&ts.scr, &box, POINT(0, 0), 4, red, blue);
  if (np_at(&ts, 8, 8) != ev || np_at(&ts, 20, 10) != back)
  {
    printf("screen: fill_checks ignored the screen clip\n");
    return result_TEST_FAILED;
  }

  /* a non-positive size draws nothing */
  testscreen_init(&ts);
  screen_fill_checks(&ts.scr, &box, POINT(0, 0), 0, red, blue);
  if (np_at(&ts, 8, 8) != back)
  {
    printf("screen: fill_checks drew with size 0\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

static result_t test_fill_linear_gradient(void)
{
  static testscreen_t ts;
  static testscreen_t enc;

  const int back = (int) (pixelfmt_bgrx8888_t) BACKGROUND;

  box_t     box = { 4, 4, 24, 6 };
  colour_t  red, blue;
  int       lo, mid, hi;

  red  = colour_rgb(255, 0, 0);
  blue = colour_rgb(0, 0, 255);
  lo   = (int) np_encode(&enc, 255, 0, 0);
  mid  = (int) np_encode(&enc, 127, 0, 128);
  hi   = (int) np_encode(&enc, 0, 0, 255);

  /* red up to x = 8, blue from x = 16, step 128 of 255 at x = 12 */
  testscreen_init(&ts);
  screen_fill_linear_gradient(&ts.scr, &box, POINT(8, 0), POINT(16, 0),
                              red, blue);
  if (np_at(&ts, 4, 4)  != lo  ||
      np_at(&ts, 8, 5)  != lo  ||
      np_at(&ts, 12, 4) != mid ||
      np_at(&ts, 16, 5) != hi  ||
      np_at(&ts, 23, 4) != hi)
  {
    printf("screen: fill_linear_gradient wrong ramp\n");
    return result_TEST_FAILED;
  }
  if (np_at(&ts, 3, 4) != back || np_at(&ts, 24, 4) != back ||
      np_at(&ts, 10, 6) != back)
  {
    printf("screen: fill_linear_gradient drew outside the box\n");
    return result_TEST_FAILED;
  }

  /* honours the screen clip */
  testscreen_init(&ts);
  ts.scr.clip = (box_t) { 0, 0, 10, 64 };
  screen_fill_linear_gradient(&ts.scr, &box, POINT(8, 0), POINT(16, 0),
                              red, blue);
  if (np_at(&ts, 9, 4) == back || np_at(&ts, 10, 4) != back)
  {
    printf("screen: fill_linear_gradient ignored the screen clip\n");
    return result_TEST_FAILED;
  }

  /* a == b draws nothing */
  testscreen_init(&ts);
  screen_fill_linear_gradient(&ts.scr, &box, POINT(8, 0), POINT(8, 0),
                              red, blue);
  if (np_at(&ts, 8, 4) != back)
  {
    printf("screen: fill_linear_gradient drew with a == b\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

/* Every pixel of gradients at assorted angles and lengths takes the step
 * that projecting it onto a to b and dividing gives. */
static result_t test_fill_linear_gradient_steps(void)
{
  static testscreen_t ts;

  const box_t whole = { 0, 0, WIDTH, HEIGHT };

  colour_t     red, blue;
  unsigned int seed;
  int          n;
  point_t      a, b;
  double       dx, dy, len2;
  int          x, y;
  int          i;

  red  = colour_rgb(255, 0, 0);
  blue = colour_rgb(0, 0, 255);
  seed = 12345;

  for (n = 0; n < 300; n++)
  {
    seed = seed * 1103515245u + 12345u;
    a.x  = (int) (seed >> 16) % 260 - 100;
    seed = seed * 1103515245u + 12345u;
    a.y  = (int) (seed >> 16) % 260 - 100;
    seed = seed * 1103515245u + 12345u;
    b.x  = (int) (seed >> 16) % 260 - 100;
    seed = seed * 1103515245u + 12345u;
    b.y  = (int) (seed >> 16) % 260 - 100;
    if (n % 3 == 0)
      b.x = a.x + (b.x & 3); /* short ramps, many steps per pixel */

    dx   = b.x - a.x;
    dy   = b.y - a.y;
    len2 = dx * dx + dy * dy;
    if (len2 == 0)
      continue;

    testscreen_init(&ts);
    screen_fill_linear_gradient(&ts.scr, &whole, a, b, red, blue);

    for (y = 0; y < HEIGHT; y++)
      for (x = 0; x < WIDTH; x++)
      {
        i = CLAMP((int) (((x - a.x) * dx + (y - a.y) * dy) / len2 * 255 +
                         0.5), 0, 255);
        if (np_at(&ts, x, y) !=
            (int) screen_colour_to_pixel(&ts.scr,
                                         colour_mix_rgba(red, blue, i, 255)))
        {
          printf("screen: fill_linear_gradient (%d,%d)-(%d,%d) wrong step at "
                 "%d,%d\n", a.x, a.y, b.x, b.y, x, y);
          return result_TEST_FAILED;
        }
      }
  }

  return result_TEST_PASSED;
}

/* ----------------------------------------------------------------------- */

static result_t test_dashed_line(void)
{
  static testscreen_t ts;
  static testscreen_t enc;

  int                 fg;
  int                 x;
  int                 on_count, off_count;

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

static result_t test_draw_lines(void)
{
  static testscreen_t ts;
  static testscreen_t enc;

  const point_t chain[] = { { 4, 4 }, { 20, 4 }, { 20, 20 } };
  int           fg, bg;
  int           x, y;

  fg = (int) np_encode(&enc, 255, 0, 0);
  bg = (int) (pixelfmt_bgrx8888_t) BACKGROUND;

  /* An open two-segment polyline: both segments drawn, the shared joint
   * pixel lit, nothing else. */
  testscreen_init(&ts);
  screen_draw_lines(&ts.scr, chain, NELEMS(chain), colour_rgb(255, 0, 0));

  for (x = 4; x <= 20; x++)
    if (np_at(&ts, x, 4) != fg)
    {
      printf("screen: draw_lines gap on segment 1 at x=%d\n", x);
      return result_TEST_FAILED;
    }
  for (y = 4; y <= 20; y++)
    if (np_at(&ts, 20, y) != fg)
    {
      printf("screen: draw_lines gap on segment 2 at y=%d\n", y);
      return result_TEST_FAILED;
    }
  if (np_at(&ts, 5, 5) != bg || np_at(&ts, 4, 20) != bg)
  {
    printf("screen: draw_lines drew outside the polyline\n");
    return result_TEST_FAILED;
  }

  /* Fewer than two points is a no-op. */
  testscreen_init(&ts);
  screen_draw_lines(&ts.scr, chain, 1, colour_rgb(255, 0, 0));
  screen_draw_lines(&ts.scr, NULL, 5, colour_rgb(255, 0, 0));
  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
      if (np_at(&ts, x, y) != bg)
      {
        printf("screen: draw_lines with <2 points drew at (%d,%d)\n", x, y);
        return result_TEST_FAILED;
      }

  return result_TEST_PASSED;
}

/* ----------------------------------------------------------------------- */

static result_t test_draw_thick_line(void)
{
  static testscreen_t ts;
  static testscreen_t enc;

  int                 fg, bg;
  int                 x, y;
  int                 lit;

  fg = (int) np_encode(&enc, 255, 0, 0);
  bg = (int) (pixelfmt_bgrx8888_t) BACKGROUND;

  /* Horizontal, width 3: rows 19..21, columns 10..30 inclusive, no more. */
  testscreen_init(&ts);
  screen_draw_thick_line(&ts.scr, 10, 20, 30, 20, 3, colour_rgb(255, 0, 0));

  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
    {
      int want;

      want = (y >= 19 && y <= 21 && x >= 10 && x <= 30) ? fg : bg;
      if (np_at(&ts, x, y) != want)
      {
        printf("screen: draw_thick_line horizontal wrong at (%d,%d)\n", x, y);
        return result_TEST_FAILED;
      }
    }

  /* Vertical, width 2, drawn backwards: columns 19..20, rows 5..25. */
  testscreen_init(&ts);
  screen_draw_thick_line(&ts.scr, 20, 25, 20, 5, 2, colour_rgb(255, 0, 0));

  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
    {
      int want;

      want = (x >= 19 && x <= 20 && y >= 5 && y <= 25) ? fg : bg;
      if (np_at(&ts, x, y) != want)
      {
        printf("screen: draw_thick_line vertical wrong at (%d,%d)\n", x, y);
        return result_TEST_FAILED;
      }
    }

  /* Diagonal, width 5: the centreline and its neighbours across the stroke
   * are lit, pixels well off it are not. */
  testscreen_init(&ts);
  screen_draw_thick_line(&ts.scr, 10, 10, 50, 50, 5, colour_rgb(255, 0, 0));

  for (x = 10; x <= 50; x++)
    if (np_at(&ts, x, x) != fg ||
        (x < 50 && (np_at(&ts, x + 1, x) != fg || np_at(&ts, x, x + 1) != fg)))
    {
      printf("screen: draw_thick_line diagonal gap at %d\n", x);
      return result_TEST_FAILED;
    }
  if (np_at(&ts, 30, 36) != bg || np_at(&ts, 36, 30) != bg ||
      np_at(&ts, 6, 6) != bg || np_at(&ts, 54, 54) != bg)
  {
    printf("screen: draw_thick_line diagonal too fat\n");
    return result_TEST_FAILED;
  }

  /* Width 1 is a plain line; width 0 draws nothing. */
  testscreen_init(&ts);
  screen_draw_thick_line(&ts.scr, 4, 4, 40, 20, 0, colour_rgb(255, 0, 0));
  lit = 0;
  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
      lit += (np_at(&ts, x, y) != bg);
  if (lit != 0)
  {
    printf("screen: draw_thick_line width 0 drew\n");
    return result_TEST_FAILED;
  }

  testscreen_init(&enc);
  testscreen_init(&ts);
  screen_draw_line(&enc.scr, 4, 4, 40, 20, colour_rgb(255, 0, 0));
  screen_draw_thick_line(&ts.scr, 4, 4, 40, 20, 1, colour_rgb(255, 0, 0));
  if (memcmp(ts.pixels, enc.pixels, sizeof(ts.pixels)) != 0)
  {
    printf("screen: draw_thick_line width 1 differs from draw_line\n");
    return result_TEST_FAILED;
  }

  /* Wild and wholly off-screen coordinates must not crash or draw. */
  testscreen_init(&ts);
  screen_draw_thick_line(&ts.scr, -100000, -100000, 100000, 100000, 7,
                         colour_rgb(255, 0, 0));
  screen_draw_thick_line(&ts.scr, -50, -50, -20, -10, 6,
                         colour_rgb(255, 0, 0));
  if (np_at(&ts, 32, 32) != fg || np_at(&ts, 0, 63) != bg)
  {
    printf("screen: draw_thick_line clipped line wrong\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

/* ----------------------------------------------------------------------- */

static result_t test_draw_rect(void)
{
  static testscreen_t ts;
  static testscreen_t enc;

  int                 fg, bg;
  int                 x, y;

  fg = (int) np_encode(&enc, 0, 255, 0);
  bg = (int) (pixelfmt_bgrx8888_t) BACKGROUND;

  /* 10x8 outline at (5,5): edges lit, interior and exterior background.
   * size is inclusive, so the far edges are at x=14, y=12. */
  testscreen_init(&ts);
  screen_draw_rect(&ts.scr, 5, 5, SIZE2D(10, 8), colour_rgb(0, 255, 0));

  for (x = 5; x <= 14; x++)
    if (np_at(&ts, x, 5) != fg || np_at(&ts, x, 12) != fg)
    {
      printf("screen: draw_rect horizontal edge gap at x=%d\n", x);
      return result_TEST_FAILED;
    }
  for (y = 5; y <= 12; y++)
    if (np_at(&ts, 5, y) != fg || np_at(&ts, 14, y) != fg)
    {
      printf("screen: draw_rect vertical edge gap at y=%d\n", y);
      return result_TEST_FAILED;
    }
  if (np_at(&ts, 9, 8) != bg)      /* interior */
  {
    printf("screen: draw_rect filled its interior\n");
    return result_TEST_FAILED;
  }
  if (np_at(&ts, 4, 5) != bg || np_at(&ts, 15, 5) != bg ||
      np_at(&ts, 5, 4) != bg || np_at(&ts, 5, 13) != bg)
  {
    printf("screen: draw_rect drew outside the rect\n");
    return result_TEST_FAILED;
  }

  /* Degenerate size (<= 1 in an axis) falls back to a filled rect. */
  testscreen_init(&ts);
  screen_draw_rect(&ts.scr, 3, 3, SIZE2D(1, 6), colour_rgb(0, 255, 0));
  for (y = 3; y <= 8; y++)
    if (np_at(&ts, 3, y) != fg)
    {
      printf("screen: draw_rect degenerate fallback gap at y=%d\n", y);
      return result_TEST_FAILED;
    }

  /* Honours the screen clip. */
  testscreen_init(&ts);
  ts.scr.clip = (box_t) { 0, 0, 10, 64 };
  screen_draw_rect(&ts.scr, 5, 5, SIZE2D(10, 8), colour_rgb(0, 255, 0));
  if (np_at(&ts, 5, 5) != fg || np_at(&ts, 14, 5) != bg)
  {
    printf("screen: draw_rect ignored the screen clip\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

/* ----------------------------------------------------------------------- */

static result_t test_draw_circle(void)
{
  static testscreen_t ts;
  static testscreen_t enc;

  int                 fg, bg;

  fg = (int) np_encode(&enc, 0, 255, 0);
  bg = (int) (pixelfmt_bgrx8888_t) BACKGROUND;

  /* r=10 about (32,32): the four axis points lie on the circle, the centre
   * and a point just inside the rim do not. */
  testscreen_init(&ts);
  screen_draw_circle(&ts.scr, 32, 32, 10, colour_rgb(0, 255, 0));

  if (np_at(&ts, 42, 32) != fg || np_at(&ts, 22, 32) != fg ||
      np_at(&ts, 32, 42) != fg || np_at(&ts, 32, 22) != fg)
  {
    printf("screen: draw_circle missing an axis point\n");
    return result_TEST_FAILED;
  }
  if (np_at(&ts, 32, 32) != bg || np_at(&ts, 38, 32) != bg)
  {
    printf("screen: draw_circle filled its interior\n");
    return result_TEST_FAILED;
  }

  /* r=0 draws the centre pixel only. */
  testscreen_init(&ts);
  screen_draw_circle(&ts.scr, 5, 5, 0, colour_rgb(0, 255, 0));
  if (np_at(&ts, 5, 5) != fg || np_at(&ts, 6, 5) != bg)
  {
    printf("screen: draw_circle r=0 wrong\n");
    return result_TEST_FAILED;
  }

  /* Honours the screen clip: right half of the circle is masked off. */
  testscreen_init(&ts);
  ts.scr.clip = (box_t) { 0, 0, 32, 64 };
  screen_draw_circle(&ts.scr, 32, 32, 10, colour_rgb(0, 255, 0));
  if (np_at(&ts, 22, 32) != fg || np_at(&ts, 42, 32) != bg)
  {
    printf("screen: draw_circle ignored the screen clip\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

/* ----------------------------------------------------------------------- */

static result_t test_fill_circle(void)
{
  static testscreen_t ts;
  static testscreen_t enc;

  int                 fg, bg;

  fg = (int) np_encode(&enc, 0, 255, 0);
  bg = (int) (pixelfmt_bgrx8888_t) BACKGROUND;

  /* r=10 about (32,32): centre, rim and axis extremes lit; one pixel beyond
   * the rim is background. */
  testscreen_init(&ts);
  screen_fill_circle(&ts.scr, 32, 32, 10, colour_rgb(0, 255, 0));

  if (np_at(&ts, 32, 32) != fg ||               /* centre */
      np_at(&ts, 38, 32) != fg ||               /* interior */
      np_at(&ts, 42, 32) != fg || np_at(&ts, 22, 32) != fg ||
      np_at(&ts, 32, 42) != fg || np_at(&ts, 32, 22) != fg)
  {
    printf("screen: fill_circle left a hole\n");
    return result_TEST_FAILED;
  }
  if (np_at(&ts, 43, 32) != bg || np_at(&ts, 32, 43) != bg)
  {
    printf("screen: fill_circle spilled past the rim\n");
    return result_TEST_FAILED;
  }

  /* Honours the screen clip. */
  testscreen_init(&ts);
  ts.scr.clip = (box_t) { 0, 0, 32, 64 };
  screen_fill_circle(&ts.scr, 32, 32, 10, colour_rgb(0, 255, 0));
  if (np_at(&ts, 28, 32) != fg || np_at(&ts, 36, 32) != bg)
  {
    printf("screen: fill_circle ignored the screen clip\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

/* ----------------------------------------------------------------------- */

static result_t test_rounded_rect(void)
{
  static testscreen_t outline;
  static testscreen_t filled;
  static testscreen_t enc;

  int                 fg, bg;
  int                 x, y;

  fg = (int) np_encode(&enc, 0, 255, 0);
  bg = (int) (pixelfmt_bgrx8888_t) BACKGROUND;

  /* 21x21 at (10,10), r=5: corner centres at 15 and 25. The very corner
   * and the diagonal (11,11) lie outside the arc; edge midpoints and the
   * arc's axis points lie on it. */
  testscreen_init(&outline);
  screen_draw_rounded_rect(&outline.scr, 10, 10, SIZE2D(21, 21), 5,
                           colour_rgb(0, 255, 0));
  testscreen_init(&filled);
  screen_fill_rounded_rect(&filled.scr, 10, 10, SIZE2D(21, 21), 5,
                           colour_rgb(0, 255, 0));

  if (np_at(&outline, 20, 10) != fg || np_at(&outline, 10, 20) != fg ||
      np_at(&outline, 30, 20) != fg || np_at(&outline, 20, 30) != fg ||
      np_at(&outline, 10, 15) != fg || np_at(&outline, 12, 11) != fg)
  {
    printf("screen: draw_rounded_rect missing an edge or arc point\n");
    return result_TEST_FAILED;
  }
  if (np_at(&outline, 10, 10) != bg || np_at(&outline, 11, 11) != bg ||
      np_at(&outline, 20, 20) != bg)
  {
    printf("screen: draw_rounded_rect lit a corner or its interior\n");
    return result_TEST_FAILED;
  }

  /* the fill covers every outline pixel plus the interior, and nothing
   * outside the outline's corners or edges */
  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
      if (np_at(&outline, x, y) == fg && np_at(&filled, x, y) != fg)
      {
        printf("screen: fill_rounded_rect missed outline pixel (%d,%d)\n",
               x, y);
        return result_TEST_FAILED;
      }
  if (np_at(&filled, 20, 20) != fg || np_at(&filled, 10, 10) != bg ||
      np_at(&filled, 11, 11) != bg || np_at(&filled, 31, 20) != bg ||
      np_at(&filled, 20, 9) != bg)
  {
    printf("screen: fill_rounded_rect wrong interior or spilled\n");
    return result_TEST_FAILED;
  }

  /* an oversized radius clamps to a circle on an odd square: (10,10) is
   * outside, the edge midpoints are on it */
  testscreen_init(&filled);
  screen_fill_rounded_rect(&filled.scr, 10, 10, SIZE2D(21, 21), 100,
                           colour_rgb(0, 255, 0));
  if (np_at(&filled, 10, 10) != bg || np_at(&filled, 14, 10) != bg ||
      np_at(&filled, 20, 10) != fg || np_at(&filled, 10, 20) != fg)
  {
    printf("screen: fill_rounded_rect did not clamp its radius\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

/* ----------------------------------------------------------------------- */

static result_t test_ellipse(void)
{
  static testscreen_t outline;
  static testscreen_t filled;
  static testscreen_t enc;

  int                 fg, bg;
  int                 x, y;

  fg = (int) np_encode(&enc, 0, 255, 0);
  bg = (int) (pixelfmt_bgrx8888_t) BACKGROUND;

  /* 31x11 at (10,20) with a 2px outline: the axis extremes are lit, the box
   * corners, the centre and the pixels beyond the ends are not. */
  testscreen_init(&outline);
  screen_draw_ellipse(&outline.scr, 10, 20, SIZE2D(31, 11), 2,
                      colour_rgb(0, 255, 0));
  testscreen_init(&filled);
  screen_fill_ellipse(&filled.scr, 10, 20, SIZE2D(31, 11),
                      colour_rgb(0, 255, 0));

  if (np_at(&outline, 10, 25) != fg || np_at(&outline, 40, 25) != fg ||
      np_at(&outline, 25, 20) != fg || np_at(&outline, 25, 30) != fg ||
      np_at(&outline, 25, 21) != fg)
  {
    printf("screen: draw_ellipse missing an axis point\n");
    return result_TEST_FAILED;
  }
  if (np_at(&outline, 10, 20) != bg || np_at(&outline, 40, 30) != bg ||
      np_at(&outline, 25, 25) != bg || np_at(&outline, 25, 22) != bg ||
      np_at(&outline, 9, 25) != bg || np_at(&outline, 41, 25) != bg)
  {
    printf("screen: draw_ellipse lit a corner, its interior or beyond\n");
    return result_TEST_FAILED;
  }

  /* the fill covers every outline pixel plus the interior */
  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
      if (np_at(&outline, x, y) == fg && np_at(&filled, x, y) != fg)
      {
        printf("screen: fill_ellipse missed outline pixel (%d,%d)\n", x, y);
        return result_TEST_FAILED;
      }
  if (np_at(&filled, 25, 25) != fg || np_at(&filled, 10, 20) != bg)
  {
    printf("screen: fill_ellipse wrong interior or spilled\n");
    return result_TEST_FAILED;
  }

  /* an empty size draws nothing */
  testscreen_init(&filled);
  screen_fill_ellipse(&filled.scr, 10, 20, SIZE2D(0, 11),
                      colour_rgb(0, 255, 0));
  screen_draw_ellipse(&filled.scr, 10, 20, SIZE2D(31, 0), 1,
                      colour_rgb(0, 255, 0));
  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
      if (np_at(&filled, x, y) != bg)
      {
        printf("screen: an empty ellipse drew (%d,%d)\n", x, y);
        return result_TEST_FAILED;
      }

  return result_TEST_PASSED;
}

/* ----------------------------------------------------------------------- */

/* Blit an rgba8888 source onto a 1bpp screen: each pixel must quantise to
 * the nearer of the two palette entries and pack MSB-first, bit 7 leftmost. */
static result_t test_copy_bitmap_p1(void)
{
#define P1_ROWBYTES (WIDTH / 8)
  static unsigned char       p1pixels[P1_ROWBYTES * HEIGHT];
  static pixelfmt_rgba8888_t srcbuf[16 * 4];

  screen_t                   scr;
  bitmap_t                   src;
  colour_t                   pal[2];
  int                        x, y;

  pal[0] = colour_rgb(0x00, 0x00, 0x00);
  pal[1] = colour_rgb(0xFF, 0xFF, 0xFF);

  /* 16x4 source: left half dark (-> index 0), right half light (-> index 1) */
  for (y = 0; y < 4; y++)
    for (x = 0; x < 16; x++)
      srcbuf[y * 16 + x] = (x < 8 ? colour_rgb(0x10, 0x10, 0x10)
                                  : colour_rgb(0xF0, 0xF0, 0xF0)).primary;

  bitmap_init(&src, SIZE2D(16, 4), pixelfmt_rgba8888,
              16 * (int) sizeof(srcbuf[0]), NULL, srcbuf);

  memset(p1pixels, 0, sizeof(p1pixels));
  screen_init(&scr, SIZE2D(WIDTH, HEIGHT), pixelfmt_p1, P1_ROWBYTES, pal,
              p1pixels);

  if (screen_copy_bitmap(&scr, 0, 0, &src) != result_OK)
  {
    printf("screen: copy_bitmap to p1 screen failed\n");
    return result_TEST_FAILED;
  }

  /* row 0: byte 0 = eight dark pixels = 0x00; byte 1 = eight light = 0xFF */
  if (p1pixels[0] != 0x00 || p1pixels[1] != 0xFF)
  {
    printf("screen: p1 blit packed wrong (0x%02X 0x%02X, want 0x00 0xFF)\n",
           p1pixels[0], p1pixels[1]);
    return result_TEST_FAILED;
  }

  /* an untouched row past the source stays clear */
  if (p1pixels[4 * P1_ROWBYTES] != 0x00)
  {
    printf("screen: p1 blit spilled past the source\n");
    return result_TEST_FAILED;
  }

  /* odd x offset: the first light pixel must land in bit 7-(3&7) of byte 0 */
  memset(p1pixels, 0, sizeof(p1pixels));
  {
    pixelfmt_rgba8888_t one = colour_rgb(0xF0, 0xF0, 0xF0).primary;
    bitmap_t            dot;

    bitmap_init(&dot, SIZE2D(1, 1), pixelfmt_rgba8888,
                (int) sizeof(one), NULL, &one);
    screen_copy_bitmap(&scr, 3, 0, &dot);
    if (p1pixels[0] != (1 << (7 - 3)))
    {
      printf("screen: p1 blit odd-x packed wrong (0x%02X)\n", p1pixels[0]);
      return result_TEST_FAILED;
    }
  }

  /* screen_set_pixel: white pixel at x=2 sets bit 7-2 of byte 0 */
  memset(p1pixels, 0, sizeof(p1pixels));
  screen_set_pixel(&scr, 2, 0, pal[1]);
  if (p1pixels[0] != (1 << (7 - 2)))
  {
    printf("screen: p1 set_pixel packed wrong (0x%02X)\n", p1pixels[0]);
    return result_TEST_FAILED;
  }

  /* screen_fill_hline: 8 white pixels from x=0 fill byte 0 */
  memset(p1pixels, 0, sizeof(p1pixels));
  screen_fill_hline(&scr, 0, 1, 8, pal[1]);
  if (p1pixels[P1_ROWBYTES] != 0xFF)
  {
    printf("screen: p1 fill_hline packed wrong (0x%02X)\n",
           p1pixels[P1_ROWBYTES]);
    return result_TEST_FAILED;
  }

  /* screen_fill_pattern: solid all-bits pattern in fg colour = 0xFF row */
  memset(p1pixels, 0, sizeof(p1pixels));
  {
    pattern_t pat;
    box_t     b;

    memset(&pat, 0, sizeof(pat));
    memset(pat.bits, 0xFF, sizeof(pat.bits));
    pat.fg = pal[1];
    pat.bg = pal[0];
    b.x0 = 0; b.y0 = 2; b.x1 = 8; b.y1 = 3;
    screen_fill_pattern(&scr, &b, &pat);
    if (p1pixels[2 * P1_ROWBYTES] != 0xFF)
    {
      printf("screen: p1 fill_pattern packed wrong (0x%02X)\n",
             p1pixels[2 * P1_ROWBYTES]);
      return result_TEST_FAILED;
    }
  }

  return result_TEST_PASSED;
}

static result_t test_copy_bitmap_p2(void)
{
#define P2_ROWBYTES (WIDTH / 4)
  static unsigned char       p2pixels[P2_ROWBYTES * HEIGHT];
  static pixelfmt_rgba8888_t srcbuf[16 * 4];

  screen_t                   scr;
  bitmap_t                   src;
  colour_t                   pal[4];
  int                        x, y;

  pal[0] = colour_rgb(0x00, 0x00, 0x00);
  pal[1] = colour_rgb(0x55, 0x55, 0x55);
  pal[2] = colour_rgb(0xAA, 0xAA, 0xAA);
  pal[3] = colour_rgb(0xFF, 0xFF, 0xFF);

  /* 16x4 source: left half black (-> index 0), right half white (-> index 3) */
  for (y = 0; y < 4; y++)
    for (x = 0; x < 16; x++)
      srcbuf[y * 16 + x] = (x < 8 ? colour_rgb(0x08, 0x08, 0x08)
                                  : colour_rgb(0xF8, 0xF8, 0xF8)).primary;

  bitmap_init(&src, SIZE2D(16, 4), pixelfmt_rgba8888,
              16 * (int) sizeof(srcbuf[0]), NULL, srcbuf);

  memset(p2pixels, 0, sizeof(p2pixels));
  screen_init(&scr, SIZE2D(WIDTH, HEIGHT), pixelfmt_p2, P2_ROWBYTES, pal,
              p2pixels);

  if (screen_copy_bitmap(&scr, 0, 0, &src) != result_OK)
  {
    printf("screen: copy_bitmap to p2 screen failed\n");
    return result_TEST_FAILED;
  }

  /* row 0: bytes 0..1 = four black pixels each = 0x00; bytes 2..3 = four
   * white (index 3 = 0b11) each = 0xFF */
  if (p2pixels[0] != 0x00 || p2pixels[1] != 0x00 ||
      p2pixels[2] != 0xFF || p2pixels[3] != 0xFF)
  {
    printf("screen: p2 blit packed wrong (0x%02X 0x%02X 0x%02X 0x%02X)\n",
           p2pixels[0], p2pixels[1], p2pixels[2], p2pixels[3]);
    return result_TEST_FAILED;
  }

  /* an untouched row past the source stays clear */
  if (p2pixels[4 * P2_ROWBYTES] != 0x00)
  {
    printf("screen: p2 blit spilled past the source\n");
    return result_TEST_FAILED;
  }

  /* odd x offset: first white pixel lands in bits 6-((3&3)<<1) = 0 of byte 0 */
  memset(p2pixels, 0, sizeof(p2pixels));
  {
    pixelfmt_rgba8888_t one = colour_rgb(0xF8, 0xF8, 0xF8).primary;
    bitmap_t            dot;

    bitmap_init(&dot, SIZE2D(1, 1), pixelfmt_rgba8888,
                (int) sizeof(one), NULL, &one);
    screen_copy_bitmap(&scr, 3, 0, &dot);
    if (p2pixels[0] != (3 << 0))
    {
      printf("screen: p2 blit odd-x packed wrong (0x%02X)\n", p2pixels[0]);
      return result_TEST_FAILED;
    }
  }

  /* screen_set_pixel: white pixel at x=1 sets bits 6-((1&3)<<1) = 4 of byte 0 */
  memset(p2pixels, 0, sizeof(p2pixels));
  screen_set_pixel(&scr, 1, 0, pal[3]);
  if (p2pixels[0] != (3 << 4))
  {
    printf("screen: p2 set_pixel packed wrong (0x%02X)\n", p2pixels[0]);
    return result_TEST_FAILED;
  }

  /* screen_fill_hline: 4 white pixels from x=0 fill byte 0 */
  memset(p2pixels, 0, sizeof(p2pixels));
  screen_fill_hline(&scr, 0, 1, 4, pal[3]);
  if (p2pixels[P2_ROWBYTES] != 0xFF)
  {
    printf("screen: p2 fill_hline packed wrong (0x%02X)\n",
           p2pixels[P2_ROWBYTES]);
    return result_TEST_FAILED;
  }

  /* screen_fill_pattern: solid all-bits pattern in fg colour = 0xFF row */
  memset(p2pixels, 0, sizeof(p2pixels));
  {
    pattern_t pat;
    box_t     b;

    memset(&pat, 0, sizeof(pat));
    memset(pat.bits, 0xFF, sizeof(pat.bits));
    pat.fg = pal[3];
    pat.bg = pal[0];
    b.x0 = 0; b.y0 = 2; b.x1 = 4; b.y1 = 3;
    screen_fill_pattern(&scr, &b, &pat);
    if (p2pixels[2 * P2_ROWBYTES] != 0xFF)
    {
      printf("screen: p2 fill_pattern packed wrong (0x%02X)\n",
             p2pixels[2 * P2_ROWBYTES]);
      return result_TEST_FAILED;
    }
  }

  return result_TEST_PASSED;
}

#define RLE_SRC_W 24
#define RLE_SRC_H 8

/* Blit an RLE-compressed rgba8888 source and its raw twin onto a screen of
 * the given format at a spread of offsets, some clipped by every edge, over a
 * non-zero background. The two screens must match bit for bit: that covers
 * the pixel packing, neighbour pixels staying untouched, clipping, and
 * transparent pixels and skip runs leaving the background alone. The source
 * is colourful (so a colour palette is exercised, not just greys) with a
 * transparent diagonal and column band, so the codec emits skip runs as well
 * as literal ones, and a translucent band, which a deep screen blends.
 * "name" labels failures; "pal" is NULL for a deep screen. Returns 0 on
 * success. */
static int rle_blit_matches_raw(const char *name,
                                pixelfmt_t  fmt,
                                colour_t   *pal)
{
  static const int           offsets[][2] =
  {
    {  0,  0 }, {  1,  1 }, {  3,  2 }, {  5,  1 }, {  7,  5 },
    { -4, -2 }, { -12, 3 }, { 2, -6 }, { WIDTH - 6, HEIGHT - 3 },
    { WIDTH - 1, 0 }
  };
  static pixelfmt_rgba8888_t rawpix[RLE_SRC_W * RLE_SRC_H];
  static unsigned char       ref[WIDTH * HEIGHT * 2];
  static unsigned char       rle[WIDTH * HEIGHT * 2];

  int                        rowbytes;
  screen_t                   sref, srle;
  bitmap_t                   raw, comp;
  void                      *blob;
  int                        x, y;
  size_t                     o;
  int                        changed;

  rowbytes = (WIDTH << pixelfmt_log2bpp(fmt)) >> 3;

  for (y = 0; y < RLE_SRC_H; y++)
  {
    for (x = 0; x < RLE_SRC_W; x++)
    {
      int r, g, b, alpha;

      r = ((x / 3) * 37 + y * 91) & 0xFF;
      g = ((x / 3) * 53 + y * 29) & 0xFF;
      b = ((x / 3) * 11 + y * 71) & 0xFF;
      if ((x + y) % 5 == 0 || (x >= 9 && x <= 14))
        alpha = 0;
      else if (x >= 18 && x <= 20)
        alpha = 0x80;
      else
        alpha = 0xFF;
      rawpix[y * RLE_SRC_W + x] = colour_rgba(r, g, b, alpha).primary;
    }
  }

  bitmap_init(&raw, SIZE2D(RLE_SRC_W, RLE_SRC_H), pixelfmt_rgba8888,
              RLE_SRC_W * (int) sizeof(rawpix[0]), NULL, rawpix);

  blob = malloc(sizeof(rawpix));
  if (blob == NULL)
    return 1;
  memcpy(blob, rawpix, sizeof(rawpix));
  bitmap_init(&comp, SIZE2D(RLE_SRC_W, RLE_SRC_H), pixelfmt_rgba8888,
              RLE_SRC_W * (int) sizeof(rawpix[0]), NULL, blob);
  if (bitmap_compress(&comp) != result_OK || !bitmap_is_compressed(&comp))
  {
    printf("screen: rle %s: compress failed\n", name);
    free(comp.base);
    return 1;
  }

  screen_init(&sref, SIZE2D(WIDTH, HEIGHT), fmt, rowbytes, pal, ref);
  screen_init(&srle, SIZE2D(WIDTH, HEIGHT), fmt, rowbytes, pal, rle);

  for (o = 0; o < NELEMS(offsets); o++)
  {
    memset(ref, 0xA5, sizeof(ref));
    memset(rle, 0xA5, sizeof(rle));

    if (screen_copy_bitmap(&sref, offsets[o][0], offsets[o][1], &raw)
        != result_OK ||
        screen_copy_bitmap(&srle, offsets[o][0], offsets[o][1], &comp)
        != result_OK)
    {
      printf("screen: rle %s: blit at (%d,%d) failed\n",
             name, offsets[o][0], offsets[o][1]);
      free(comp.base);
      return 1;
    }

    if (memcmp(ref, rle, sizeof(ref)) != 0)
    {
      printf("screen: rle %s: blit at (%d,%d) differs from raw\n",
             name, offsets[o][0], offsets[o][1]);
      free(comp.base);
      return 1;
    }

    /* Guard against a vacuous pass: every offset shows part of the source,
     * so the screen must have changed from its background. */
    changed = 0;
    for (x = 0; x < (int) sizeof(rle); x++)
      changed |= rle[x] != 0xA5;
    if (!changed)
    {
      printf("screen: rle %s: blit at (%d,%d) drew nothing\n",
             name, offsets[o][0], offsets[o][1]);
      free(comp.base);
      return 1;
    }
  }

  free(comp.base);

  return 0;
}

/* RLE source onto 1bpp screens: black on white, then inverted, so index 0
 * is the light colour. */
static result_t test_copy_bitmap_rle_p1(void)
{
  colour_t pal[2];
  int      bad;

  pal[0] = colour_rgb(0x00, 0x00, 0x00);
  pal[1] = colour_rgb(0xFF, 0xFF, 0xFF);
  bad    = rle_blit_matches_raw("p1", pixelfmt_p1, pal);

  pal[0] = colour_rgb(0xFF, 0xFF, 0xFF);
  pal[1] = colour_rgb(0x00, 0x00, 0x00);
  bad   |= rle_blit_matches_raw("p1 inverted", pixelfmt_p1, pal);

  return bad == 0 ? result_TEST_PASSED : result_TEST_FAILED;
}

/* RLE source onto 2bpp screens: a grey ramp, then four colours. */
static result_t test_copy_bitmap_rle_p2(void)
{
  colour_t pal[4];
  int      bad;

  pal[0] = colour_rgb(0x00, 0x00, 0x00);
  pal[1] = colour_rgb(0x55, 0x55, 0x55);
  pal[2] = colour_rgb(0xAA, 0xAA, 0xAA);
  pal[3] = colour_rgb(0xFF, 0xFF, 0xFF);
  bad    = rle_blit_matches_raw("p2", pixelfmt_p2, pal);

  pal[0] = colour_rgb(0xFF, 0x00, 0x00);
  pal[1] = colour_rgb(0x00, 0xFF, 0x00);
  pal[2] = colour_rgb(0x00, 0x00, 0xFF);
  pal[3] = colour_rgb(0xFF, 0xFF, 0x00);
  bad   |= rle_blit_matches_raw("p2 colour", pixelfmt_p2, pal);

  return bad == 0 ? result_TEST_PASSED : result_TEST_FAILED;
}

/* RLE source onto a 4bpp screen: sixteen greys. */
static result_t test_copy_bitmap_rle_p4(void)
{
  colour_t pal[16];
  int      i;

  for (i = 0; i < 16; i++)
    pal[i] = colour_rgb(i * 17, i * 17, i * 17);

  return rle_blit_matches_raw("p4", pixelfmt_p4, pal) == 0
         ? result_TEST_PASSED : result_TEST_FAILED;
}

/* RLE source onto an 8bpp screen: a 3-3-2 colour cube. */
static result_t test_copy_bitmap_rle_p8(void)
{
  colour_t pal[256];
  int      i;

  for (i = 0; i < 256; i++)
    pal[i] = colour_rgb(((i >> 5) & 7) * 255 / 7,
                        ((i >> 2) & 7) * 255 / 7,
                        (i & 3) * 255 / 3);

  return rle_blit_matches_raw("p8", pixelfmt_p8, pal) == 0
         ? result_TEST_PASSED : result_TEST_FAILED;
}

/* RLE source onto the two 16bpp deep screens, where the translucent band is
 * blended rather than alpha-tested. */
static result_t test_copy_bitmap_rle_16(void)
{
  int bad;

  bad  = rle_blit_matches_raw("rgb565", pixelfmt_rgb565, NULL);
  bad |= rle_blit_matches_raw("rgbx5551", pixelfmt_rgbx5551, NULL);

  return bad == 0 ? result_TEST_PASSED : result_TEST_FAILED;
}

/* Blit an rgba8888 source onto an 8bpp paletted screen: each pixel quantises
 * to the nearest palette entry and is stored as one plain byte. Also covers
 * set_pixel / fill_hline / fill_pattern / blend_pixel on the p8 path. */
static result_t test_copy_bitmap_p8(void)
{
#define P8_ROWBYTES (WIDTH)
  static unsigned char       p8pixels[P8_ROWBYTES * HEIGHT];
  static pixelfmt_rgba8888_t srcbuf[16 * 4];

  screen_t                   scr;
  bitmap_t                   src;
  colour_t                   pal[256];
  int                        x, y;

  /* a grey ramp: entry i is (i, i, i), so nearest-match is predictable */
  for (x = 0; x < 256; x++)
    pal[x] = colour_rgb(x, x, x);

  /* 16x4 source: left half near-black (-> index ~16), right half near-white
   * (-> index ~240) */
  for (y = 0; y < 4; y++)
    for (x = 0; x < 16; x++)
      srcbuf[y * 16 + x] = (x < 8 ? colour_rgb(0x10, 0x10, 0x10)
                                  : colour_rgb(0xF0, 0xF0, 0xF0)).primary;

  bitmap_init(&src, SIZE2D(16, 4), pixelfmt_rgba8888,
              16 * (int) sizeof(srcbuf[0]), NULL, srcbuf);

  memset(p8pixels, 0, sizeof(p8pixels));
  screen_init(&scr, SIZE2D(WIDTH, HEIGHT), pixelfmt_p8, P8_ROWBYTES, pal,
              p8pixels);

  if (screen_copy_bitmap(&scr, 0, 0, &src) != result_OK)
  {
    printf("screen: copy_bitmap to p8 screen failed\n");
    return result_TEST_FAILED;
  }

  /* row 0: cols 0..7 quantise near 0x10, cols 8..15 near 0xF0. The pixelmap
   * quantises via 5:6:5 before the nearest-ramp lookup, so allow a small
   * slop rather than demanding the exact byte. */
  for (x = 0; x < 8; x++)
  {
    if (p8pixels[x] > 0x20)
    {
      printf("screen: p8 blit dark col x=%d too bright (0x%02X)\n",
             x, p8pixels[x]);
      return result_TEST_FAILED;
    }
    if (p8pixels[x + 8] < 0xE0)
    {
      printf("screen: p8 blit light col x=%d too dark (0x%02X)\n",
             x + 8, p8pixels[x + 8]);
      return result_TEST_FAILED;
    }
  }

  /* an untouched row past the source stays clear */
  for (x = 0; x < 16; x++)
    if (p8pixels[4 * P8_ROWBYTES + x] != 0x00)
    {
      printf("screen: p8 blit spilled past the source\n");
      return result_TEST_FAILED;
    }

  /* screen_set_pixel: entry 200 at x=5 lands as a plain byte */
  memset(p8pixels, 0, sizeof(p8pixels));
  screen_set_pixel(&scr, 5, 0, pal[200]);
  if (p8pixels[5] != 200)
  {
    printf("screen: p8 set_pixel wrong (0x%02X)\n", p8pixels[5]);
    return result_TEST_FAILED;
  }

  /* screen_fill_hline: 6 pixels of entry 128 from x=2 on row 1 */
  memset(p8pixels, 0, sizeof(p8pixels));
  screen_fill_hline(&scr, 2, 1, 6, pal[128]);
  for (x = 2; x < 8; x++)
    if (p8pixels[P8_ROWBYTES + x] != 128)
    {
      printf("screen: p8 fill_hline wrong at x=%d (0x%02X)\n",
             x, p8pixels[P8_ROWBYTES + x]);
      return result_TEST_FAILED;
    }
  if (p8pixels[P8_ROWBYTES + 1] != 0 || p8pixels[P8_ROWBYTES + 8] != 0)
  {
    printf("screen: p8 fill_hline smeared past its run\n");
    return result_TEST_FAILED;
  }

  /* screen_fill_pattern: solid all-bits pattern in fg = entry 77 */
  memset(p8pixels, 0, sizeof(p8pixels));
  {
    pattern_t pat;
    box_t     b;

    memset(&pat, 0, sizeof(pat));
    memset(pat.bits, 0xFF, sizeof(pat.bits));
    pat.fg = pal[77];
    pat.bg = pal[0];
    b.x0 = 0; b.y0 = 2; b.x1 = 8; b.y1 = 3;
    screen_fill_pattern(&scr, &b, &pat);
    for (x = 0; x < 8; x++)
      if (p8pixels[2 * P8_ROWBYTES + x] != 77)
      {
        printf("screen: p8 fill_pattern wrong at x=%d (0x%02X)\n",
               x, p8pixels[2 * P8_ROWBYTES + x]);
        return result_TEST_FAILED;
      }
  }

  /* screen_copy_rect on p8 (the whole-byte bytes path, bpp=1): lay down a
   * distinct-per-column run, slide it right by 5, check it moved intact and
   * did not smear past its destination */
  memset(p8pixels, 0, sizeof(p8pixels));
  for (x = 0; x < 8; x++)
    screen_set_pixel(&scr, x, 0, pal[1 + x]);
  {
    box_t s = { 0, 0, 8, 1 };
    box_t got;

    if (screen_copy_rect(&scr, &s, POINT(5, 0), &got) != result_OK)
    {
      printf("screen: copy_rect declined a p8 screen\n");
      return result_TEST_FAILED;
    }
    for (x = 0; x < 8; x++)
      if (p8pixels[x + 5] != 1 + x)
      {
        printf("screen: copy_rect p8 wrong pixel at x=%d (got %d want %d)\n",
               x + 5, p8pixels[x + 5], 1 + x);
        return result_TEST_FAILED;
      }
    if (p8pixels[13] != 0)
    {
      printf("screen: copy_rect p8 smeared past its destination\n");
      return result_TEST_FAILED;
    }
  }

  /* screen_blend_pixel via an anti-aliased diagonal: the p8 blend path must
   * write *some* mid-ramp index between the black background and white line,
   * not leave the pixel at 0 and not slam it to 255 */
  memset(p8pixels, 0, sizeof(p8pixels));
  screen_draw_line_wu_float(&scr, 0.0f, 0.0f, 8.0f, 3.0f,
                            colour_rgb(0xFF, 0xFF, 0xFF));
  {
    int seen_partial = 0;

    for (y = 0; y < 4; y++)
      for (x = 0; x < 9; x++)
      {
        unsigned char v = p8pixels[y * P8_ROWBYTES + x];
        if (v > 0 && v < 255)
          seen_partial = 1;
      }
    if (!seen_partial)
    {
      printf("screen: p8 blend_pixel produced no partial-coverage pixel\n");
      return result_TEST_FAILED;
    }
  }

  return result_TEST_PASSED;
#undef P8_ROWBYTES
}

/* screen_copy_bitmap with a bgra8888 / bgrx8888 source onto an rgbx8888
 * screen: the source's R and B must be swapped back, so red lands as red */
static result_t test_copy_bitmap_bgr_source(void)
{
  static const pixelfmt_t fmts[] = { pixelfmt_bgra8888, pixelfmt_bgrx8888 };
  static pixelfmt_any32_t scrbuf[WIDTH * HEIGHT];

  pixelfmt_any32_t        srcpx;
  colour_t                red;
  screen_t                scr;
  bitmap_t                src;
  size_t                  i;

  srcpx = PIXELFMT_MAKE_BGRA8888(0xFF, 0x00, 0x00, 0xFF);
  red   = colour_rgb(0xFF, 0x00, 0x00);

  for (i = 0; i < NELEMS(fmts); i++)
  {
    bitmap_init(&src, SIZE2D(1, 1), fmts[i], (int) sizeof(srcpx), NULL,
                &srcpx);

    memset(scrbuf, 0, sizeof(scrbuf));
    screen_init(&scr, SIZE2D(WIDTH, HEIGHT), pixelfmt_rgbx8888,
                WIDTH * (int) sizeof(scrbuf[0]), NULL, scrbuf);

    if (screen_copy_bitmap(&scr, 0, 0, &src) != result_OK ||
        scrbuf[0] != red.primary)
    {
      printf("screen: bgr-source blit (fmt %d) got 0x%08X want 0x%08X\n",
             (int) fmts[i], scrbuf[0], red.primary);
      return result_TEST_FAILED;
    }
  }

  return result_TEST_PASSED;
}

/* screen_copy_bitmap_faded of a raw and an RLE source, an opaque red pixel
 * then a transparent one, toward white at 0, 128 and 256: the opaque pixel
 * mixes by amount/256 and the transparent one leaves the screen alone. */
static result_t test_copy_bitmap_faded(void)
{
  static const int        amounts[] = { 0, 128, 256 };
  static const int        mixed[]   = { 0, 127, 255 };
  static pixelfmt_any32_t scrbuf[WIDTH * HEIGHT];

  pixelfmt_rgba8888_t     rawpix[2];
  pixelfmt_rgba8888_t    *blob;
  bitmap_t                raw;
  bitmap_t                comp;
  colour_t                white;
  colour_t                bg;
  screen_t                scr;
  int                     rle;
  size_t                  i;
  colour_t                want;

  rawpix[0] = colour_rgba(0xFF, 0x00, 0x00, 0xFF).primary;
  rawpix[1] = colour_rgba(0x00, 0x00, 0x00, 0x00).primary; /* as RLE skips */
  bitmap_init(&raw, SIZE2D(2, 1), pixelfmt_rgba8888,
              (int) sizeof(rawpix), NULL, rawpix);

  blob = malloc(sizeof(rawpix));
  if (blob == NULL)
    return result_OOM;
  memcpy(blob, rawpix, sizeof(rawpix));
  bitmap_init(&comp, SIZE2D(2, 1), pixelfmt_rgba8888,
              (int) sizeof(rawpix), NULL, blob);
  if (bitmap_compress(&comp) != result_OK)
  {
    free(comp.base);
    return result_TEST_FAILED;
  }

  white = colour_rgb(0xFF, 0xFF, 0xFF);
  bg    = colour_rgb(0x00, 0x80, 0x00);

  screen_init(&scr, SIZE2D(WIDTH, HEIGHT), pixelfmt_rgbx8888,
              WIDTH * (int) sizeof(scrbuf[0]), NULL, scrbuf);

  for (rle = 0; rle < 2; rle++)
  {
    for (i = 0; i < NELEMS(amounts); i++)
    {
      scrbuf[0] = bg.primary;
      scrbuf[1] = bg.primary;
      want      = colour_rgb(0xFF, mixed[i], mixed[i]);

      if (screen_copy_bitmap_faded(&scr, 0, 0, rle ? &comp : &raw, white,
                                   amounts[i]) != result_OK ||
          scrbuf[0] != want.primary ||
          scrbuf[1] != bg.primary)
      {
        printf("screen: faded (rle %d, amount %d) got 0x%08X 0x%08X want "
               "0x%08X 0x%08X\n", rle, amounts[i], scrbuf[0], scrbuf[1],
               want.primary, bg.primary);
        free(comp.base);
        return result_TEST_FAILED;
      }
    }
  }

  free(comp.base);

  return result_TEST_PASSED;
}

/* screen_copy_bitmap of each deep 32bpp source onto each deep screen, wider
 * than a blit chunk and starting off the screen's left edge: every pixel must
 * be what colour_to_pixel makes of it, or the background where an alpha
 * source is transparent. An x source is opaque whatever its alpha byte. */
static result_t test_copy_bitmap_deep_formats(void)
{
#define DF_W 300
#define DF_SCR_W 320
#define DF_X (-5)
  static const pixelfmt_t srcfmts[] =
  {
    pixelfmt_rgba8888, pixelfmt_rgbx8888, pixelfmt_bgra8888, pixelfmt_bgrx8888
  };
  static const pixelfmt_t scrfmts[] =
  {
    pixelfmt_bgrx8888, pixelfmt_bgra8888, pixelfmt_rgbx8888, pixelfmt_rgb565,
    pixelfmt_rgbx5551
  };
  static pixelfmt_any32_t srcbuf[DF_W];
  static pixelfmt_any32_t scrbuf[DF_SCR_W];

  unsigned int            seed;
  size_t                  i, j;
  int                     x;
  int                     bpp;
  int                     alpha, bgr;
  bitmap_t                src;
  screen_t                scr;
  pixelfmt_any32_t        px;
  colour_t                c;
  pixelfmt_any32_t        got, want;

  seed = 1;
  for (x = 0; x < DF_W; x++)
  {
    seed      = seed * 1103515245u + 12345u;
    srcbuf[x] = (seed >> 8) & 0x00FFFFFFu;
    if (x % 3 != 0)
      srcbuf[x] |= 0xFF000000u;
  }

  for (i = 0; i < NELEMS(srcfmts); i++)
    for (j = 0; j < NELEMS(scrfmts); j++)
    {
      bpp   = pixelfmt_log2bpp(scrfmts[j]) == 5 ? 4 : 2;
      alpha = pixelfmt_has_alpha(srcfmts[i]);
      bgr   = srcfmts[i] == pixelfmt_bgra8888 ||
              srcfmts[i] == pixelfmt_bgrx8888;

      bitmap_init(&src, SIZE2D(DF_W, 1), srcfmts[i], (int) sizeof(srcbuf),
                  NULL, srcbuf);
      memset(scrbuf, 0x5A, sizeof(scrbuf));
      screen_init(&scr, SIZE2D(DF_SCR_W, 1), scrfmts[j], DF_SCR_W * bpp, NULL,
                  scrbuf);

      if (screen_copy_bitmap(&scr, DF_X, 0, &src) != result_OK)
        return result_TEST_FAILED;

      for (x = 0; x < DF_SCR_W; x++)
      {
        got  = bpp == 4 ? scrbuf[x] : ((unsigned short *) scrbuf)[x];
        want = bpp == 4 ? 0x5A5A5A5Au : 0x5A5Au;
        if (x < DF_W + DF_X)
        {
          px = srcbuf[x - DF_X];
          if (bgr)
            px = PIXELFMT_MAKE_RGBA8888(PIXELFMT_xxRx8888(px),
                                        PIXELFMT_xGxx8888(px),
                                        PIXELFMT_Bxxx8888(px),
                                        PIXELFMT_xxxA8888(px));
          if (!alpha)
            px |= 0xFF000000u; /* an x source is opaque */
          c.primary = px;
          if (!alpha || colour_get_alpha(&c) != 0)
            want = colour_to_pixel(NULL, 0, c, scrfmts[j]);
        }
        if (got != want)
        {
          printf("screen: deep blit %d to %d at %d got 0x%08X want 0x%08X\n",
                 (int) srcfmts[i], (int) scrfmts[j], x, got, want);
          return result_TEST_FAILED;
        }
      }
    }

  return result_TEST_PASSED;
#undef DF_X
#undef DF_SCR_W
#undef DF_W
}

/* screen_copy_bitmap with a *paletted* source (p4) onto a 32bpp screen: the
 * unpack path in screen_copy_bitmap_i must decode the packed source indices
 * through its own palette before the 32bpp blit runs, landing exact colours
 * -- unlike the deep-source tests above there is no quantisation step to
 * allow slop for. */
static result_t test_copy_bitmap_paletted_source(void)
{
#define P4_SRC_ROWBYTES (8 / 2)
  static unsigned char    p4srcbuf[P4_SRC_ROWBYTES * 2];
  static pixelfmt_any32_t scrbuf[WIDTH * HEIGHT];

  screen_t                scr;
  bitmap_t                src;
  colour_t                srcpal[16];
  colour_t                got;
  int                     x;

  /* row 0: indices 0..7 packed two to the byte -- even x in the low nibble,
   * odd x in the high nibble (matches screen_set_pixel_p4's own bit order) */
  for (x = 0; x < 8; x++)
    p4srcbuf[x >> 1] |= (unsigned char) (x << ((x & 1) ? 4 : 0));
  memset(p4srcbuf + P4_SRC_ROWBYTES, 0, P4_SRC_ROWBYTES); /* row 1: all index 0 */

  for (x = 0; x < 16; x++)
    srcpal[x] = colour_rgb((unsigned char) (x * 16),
                           (unsigned char) (x * 8),
                           (unsigned char) (255 - x * 16));

  bitmap_init(&src, SIZE2D(8, 2), pixelfmt_p4, P4_SRC_ROWBYTES, srcpal,
              p4srcbuf);

  memset(scrbuf, 0, sizeof(scrbuf));
  screen_init(&scr, SIZE2D(WIDTH, HEIGHT), pixelfmt_rgbx8888,
             WIDTH * (int) sizeof(scrbuf[0]), NULL, scrbuf);

  if (screen_copy_bitmap(&scr, 2, 3, &src) != result_OK)
  {
    printf("screen: copy_bitmap of a p4 source onto a 32bpp screen failed\n");
    return result_TEST_FAILED;
  }

  for (x = 0; x < 8; x++)
  {
    got.primary = scrbuf[3 * WIDTH + 2 + x];
    if (got.primary != srcpal[x].primary)
    {
      printf("screen: paletted-source blit wrong at x=%d "
            "(got 0x%08X want 0x%08X)\n",
            x, got.primary, srcpal[x].primary);
      return result_TEST_FAILED;
    }
  }

  /* a pixel just outside the blitted box on every side stays clear, i.e. the
   * unpack scratch buffer's extent matches the clipped draw box exactly */
  if (scrbuf[3 * WIDTH + 1] != 0 || scrbuf[3 * WIDTH + 10] != 0 ||
      scrbuf[2 * WIDTH + 2] != 0 || scrbuf[5 * WIDTH + 2] != 0)
  {
    printf("screen: paletted-source blit spilled past its box\n");
    return result_TEST_FAILED;
  }

  /* a fully-transparent palette entry (as a PNG's tRNS chunk can produce)
   * must alpha-test out, same as a transparent rgba8888/bgra8888 source
   * pixel would: the unpack step must carry the tRNS-derived alpha through
   * rather than baking every unpacked pixel opaque */
  {
    pixelfmt_any32_t sentinel = 0xDEADBEEFu;

    srcpal[0] = colour_rgba(0xFF, 0x00, 0x00, 0x00); /* red, fully transparent */
    bitmap_set_palette(&src, srcpal); /* src.palette is its own copy */
    scrbuf[3 * WIDTH + 2] = sentinel;

    if (screen_copy_bitmap(&scr, 2, 3, &src) != result_OK)
    {
      printf("screen: copy_bitmap of a p4 source (2nd pass) failed\n");
      return result_TEST_FAILED;
    }

    if (scrbuf[3 * WIDTH + 2] != sentinel)
    {
      printf("screen: paletted-source blit drew a fully-transparent index "
            "(0x%08X, background was 0x%08X)\n",
            scrbuf[3 * WIDTH + 2], sentinel);
      return result_TEST_FAILED;
    }
  }

  return result_TEST_PASSED;
#undef P4_SRC_ROWBYTES
}

/* screen_copy_bitmap of a p4 source onto a p4 screen takes a remap fast
 * path. It must land exactly what the same image as an rgba8888 source
 * does through the general path: same nearest matches, same transparency,
 * same clipping, at even and odd destination x. */
static result_t test_copy_bitmap_p4_to_p4(void)
{
#define P4P4_W 7
#define P4P4_H 3
  static const int xs[] = { 3, 4, -2, -3, WIDTH - 4, WIDTH - 5 };

  static unsigned char       p4buf[(P4P4_W + 1) / 2 * P4P4_H];
  static pixelfmt_rgba8888_t deepbuf[P4P4_W * P4P4_H];
  static unsigned char       want[WIDTH / 2 * HEIGHT];
  static unsigned char       got[WIDTH / 2 * HEIGHT];

  colour_t                   srcpal[16];
  colour_t                   scrpal[16];
  bitmap_t                   p4src;
  bitmap_t                   deepsrc;
  int                        i, x, y;
  int                        pass;
  size_t                     xi;
  screen_t                   wantscr;
  screen_t                   gotscr;

  for (i = 0; i < 16; i++)
  {
    srcpal[i] = colour_rgb((unsigned char) (i * 37),
                           (unsigned char) (255 - i * 13),
                           (unsigned char) (i * 91));
    scrpal[i] = colour_rgb((unsigned char) ((i & 1) ? 255 : 0),
                           (unsigned char) ((i & 2) ? 255 : 0),
                           (unsigned char) ((i & 4) ? ((i & 8) ? 255 : 128) : 0));
  }
  memset(p4buf, 0, sizeof(p4buf));
  for (y = 0; y < P4P4_H; y++)
    for (x = 0; x < P4P4_W; x++)
    {
      i = (x + y * P4P4_W) & 15;
      p4buf[y * ((P4P4_W + 1) / 2) + (x >> 1)] |=
        (unsigned char) (i << ((x & 1) * 4));
      deepbuf[y * P4P4_W + x] = srcpal[i].primary;
    }

  bitmap_init(&p4src, SIZE2D(P4P4_W, P4P4_H), pixelfmt_p4,
              (P4P4_W + 1) / 2, srcpal, p4buf);
  bitmap_init(&deepsrc, SIZE2D(P4P4_W, P4P4_H), pixelfmt_rgba8888,
              P4P4_W * 4, NULL, deepbuf);

  /* opaque first, then with a fully transparent entry */
  for (pass = 0; pass < 2; pass++)
  {
    for (xi = 0; xi < NELEMS(xs); xi++)
    {
      if (pass == 1 && xi == 0)
      {
        srcpal[5] = colour_rgba(0x00, 0xFF, 0x00, 0x00);
        for (i = 0; i < P4P4_W * P4P4_H; i++)
          if ((i & 15) == 5)
            deepbuf[i] = srcpal[5].primary;
        bitmap_set_palette(&p4src, srcpal);
      }

      memset(want, 0x9A, sizeof(want));
      memset(got, 0x9A, sizeof(got));
      screen_init(&wantscr, SIZE2D(WIDTH, HEIGHT), pixelfmt_p4, WIDTH / 2,
                  scrpal, want);
      screen_init(&gotscr, SIZE2D(WIDTH, HEIGHT), pixelfmt_p4, WIDTH / 2,
                  scrpal, got);

      if (screen_copy_bitmap(&wantscr, xs[xi], 1, &deepsrc) != result_OK ||
          screen_copy_bitmap(&gotscr, xs[xi], 1, &p4src) != result_OK)
      {
        printf("screen: p4 to p4 copy_bitmap failed at x=%d pass %d\n",
               xs[xi], pass);
        bitmap_set_palette(&p4src, NULL);
        return result_TEST_FAILED;
      }

      if (memcmp(want, got, sizeof(want)) != 0)
      {
        printf("screen: p4 to p4 copy_bitmap differs from rgba8888 at x=%d "
               "pass %d\n", xs[xi], pass);
        bitmap_set_palette(&p4src, NULL);
        return result_TEST_FAILED;
      }
    }
  }

  bitmap_set_palette(&p4src, NULL);

  return result_TEST_PASSED;
#undef P4P4_H
#undef P4P4_W
}

/* Unpack the p2 pixel at column "px" of a row's byte buffer (bits 7..6 are
 * column 0, MSB-first, four to the byte). */
static int p2_pixel_at(const unsigned char *rowbuf, int px)
{
  return (rowbuf[px >> 2] >> (6 - ((px & 3) << 1))) & 3;
}

/* screen_copy_bitmap_tiled_dithered on a p2 screen: the same pixels as a grid
 * of screen_copy_bitmap_dithered copies clipped to the box, and unlike the
 * plain tiled copy of a flat mid-grey source, which is uniform. */
static result_t test_tiled_dithered(void)
{
#define TD_ROWBYTES (WIDTH / 4)
  static unsigned char       got[TD_ROWBYTES * HEIGHT];
  static unsigned char       want[TD_ROWBYTES * HEIGHT];
  static unsigned char       plain[TD_ROWBYTES * HEIGHT];
  static pixelfmt_rgba8888_t srcbuf[16 * 8];

  colour_t                   pal[4];
  int                        x, y;
  point_t                    origin;
  int                        w;
  bitmap_t                   src;
  box_t                      dst = { 5, 6, 50, 40 };
  screen_t                   scr;

  pal[0] = colour_rgb(0x00, 0x00, 0x00);
  pal[1] = colour_rgb(0x55, 0x55, 0x55);
  pal[2] = colour_rgb(0xAA, 0xAA, 0xAA);
  pal[3] = colour_rgb(0xFF, 0xFF, 0xFF);

  /* flat mid-grey (0x80): sits between pal[1] and pal[2] */
  for (y = 0; y < 8; y++)
    for (x = 0; x < 16; x++)
      srcbuf[y * 16 + x] = colour_rgb(0x80, 0x80, 0x80).primary;

  origin = POINT(-5, -3); /* off-screen, so the phase must wrap */

  /* 16x8 is pre-tiled; 12 wide is not a whole number of Bayer periods, so
   * it is pre-tiled down only */
  for (w = 16; w >= 12; w -= 4)
  {
    bitmap_init(&src, SIZE2D(w, 8), pixelfmt_rgba8888,
                16 * (int) sizeof(srcbuf[0]), NULL, srcbuf);

    memset(got, 0, sizeof(got));
    screen_init(&scr, SIZE2D(WIDTH, HEIGHT), pixelfmt_p2, TD_ROWBYTES, pal,
                got);
    if (screen_copy_bitmap_tiled_dithered(&scr, &dst, &src, origin,
                                          screen_DITHER_BAYER) != result_OK)
    {
      printf("screen: tiled_dithered copy failed\n");
      return result_TEST_FAILED;
    }

    memset(want, 0, sizeof(want));
    screen_init(&scr, SIZE2D(WIDTH, HEIGHT), pixelfmt_p2, TD_ROWBYTES, pal,
                want);
    scr.clip = dst;
    for (y = origin.y; y < dst.y1; y += src.size.h)
      for (x = origin.x; x < dst.x1; x += src.size.w)
        screen_copy_bitmap_dithered(&scr, x, y, &src, screen_DITHER_BAYER);

    if (memcmp(got, want, sizeof(got)) != 0)
    {
      printf("screen: tiled_dithered differs from a grid of dithered copies "
             "at width %d\n", w);
      return result_TEST_FAILED;
    }
  }

  memset(plain, 0, sizeof(plain));
  screen_init(&scr, SIZE2D(WIDTH, HEIGHT), pixelfmt_p2, TD_ROWBYTES, pal, plain);
  screen_copy_bitmap_tiled(&scr, &dst, &src, origin);

  if (memcmp(got, plain, sizeof(got)) == 0)
  {
    printf("screen: tiled_dithered matches the plain tiled copy\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
#undef TD_ROWBYTES
}

/* screen_copy_bitmap_dithered on a p2 screen: a flat mid-grey source, which
 * screen_copy_bitmap would quantise to one uniform index across the row, must
 * come out as a stipple of at least two indices; the flat black and white
 * ends must still be uniform (the dither nudge can't push them off their
 * clamp). Run once per dithering method. */
static result_t test_copy_bitmap_dithered_with(screen_dither_t method)
{
#define PD_ROWBYTES (WIDTH / 4)
  static unsigned char       pdpixels[PD_ROWBYTES * HEIGHT];
  static pixelfmt_rgba8888_t srcbuf[16 * 8];

  screen_t                   scr;
  bitmap_t                   src;
  colour_t                   pal[4];
  int                        first;
  int                        varied;
  int                        px;
  int                        x, y;

  pal[0] = colour_rgb(0x00, 0x00, 0x00);
  pal[1] = colour_rgb(0x55, 0x55, 0x55);
  pal[2] = colour_rgb(0xAA, 0xAA, 0xAA);
  pal[3] = colour_rgb(0xFF, 0xFF, 0xFF);

  /* 16x8 flat mid-grey (0x80): sits between pal[1] and pal[2]. */
  for (y = 0; y < 8; y++)
    for (x = 0; x < 16; x++)
      srcbuf[y * 16 + x] = colour_rgb(0x80, 0x80, 0x80).primary;

  bitmap_init(&src, SIZE2D(16, 8), pixelfmt_rgba8888,
              16 * (int) sizeof(srcbuf[0]), NULL, srcbuf);

  memset(pdpixels, 0, sizeof(pdpixels));
  screen_init(&scr, SIZE2D(WIDTH, HEIGHT), pixelfmt_p2, PD_ROWBYTES, pal,
              pdpixels);

  if (screen_copy_bitmap_dithered(&scr, 0, 0, &src, method) != result_OK)
  {
    printf("screen: copy_bitmap_dithered to p2 screen failed\n");
    return result_TEST_FAILED;
  }

  /* plain blit of the same source: every one of the first 16 pixels
   * quantises to the same index. */
  {
    unsigned char plain[PD_ROWBYTES * HEIGHT];
    screen_t      pscr;

    memset(plain, 0, sizeof(plain));
    screen_init(&pscr, SIZE2D(WIDTH, HEIGHT), pixelfmt_p2, PD_ROWBYTES, pal,
                plain);
    screen_copy_bitmap(&pscr, 0, 0, &src);
    for (px = 1; px < 16; px++)
      if (p2_pixel_at(plain, px) != p2_pixel_at(plain, 0))
      {
        printf("screen: plain p2 blit of a flat source was not uniform\n");
        return result_TEST_FAILED;
      }
  }

  /* dithered: at least one of the first 16 pixels differs from pixel 0 (the
   * threshold map varies across the row). */
  first  = p2_pixel_at(pdpixels, 0);
  varied = 0;
  for (px = 1; px < 16; px++)
    if (p2_pixel_at(pdpixels, px) != first)
      varied = 1;
  if (!varied)
  {
    printf("screen: dithered p2 blit of a flat mid-grey did not stipple\n");
    return result_TEST_FAILED;
  }

  /* flat black stays index 0 across the row, flat white stays index 3. */
  for (y = 0; y < 8; y++)
    for (x = 0; x < 16; x++)
      srcbuf[y * 16 + x] = colour_rgb(0x00, 0x00, 0x00).primary;
  memset(pdpixels, 0xAA, sizeof(pdpixels));
  screen_copy_bitmap_dithered(&scr, 0, 0, &src, method);
  if (pdpixels[0] != 0x00 || pdpixels[1] != 0x00 ||
      pdpixels[2] != 0x00 || pdpixels[3] != 0x00)
  {
    printf("screen: dithered p2 blit pushed flat black off index 0\n");
    return result_TEST_FAILED;
  }

  for (y = 0; y < 8; y++)
    for (x = 0; x < 16; x++)
      srcbuf[y * 16 + x] = colour_rgb(0xFF, 0xFF, 0xFF).primary;
  memset(pdpixels, 0x00, sizeof(pdpixels));
  screen_copy_bitmap_dithered(&scr, 0, 0, &src, method);
  if (pdpixels[0] != 0xFF || pdpixels[1] != 0xFF ||
      pdpixels[2] != 0xFF || pdpixels[3] != 0xFF)
  {
    printf("screen: dithered p2 blit pushed flat white off index 3\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

static result_t test_copy_bitmap_dithered(void)
{
  result_t rc;

  rc = test_copy_bitmap_dithered_with(screen_DITHER_BAYER);
  if (rc != result_TEST_PASSED)
    return rc;

  return test_copy_bitmap_dithered_with(screen_DITHER_BLUE_NOISE);
}

/* ----------------------------------------------------------------------- */

#define SC_W 7
#define SC_H 5

typedef struct scaledcase
{
  screen_scale_t scale;
  box_t          dst;
  box_t          clip; /* empty for none */
}
scaledcase_t;

/* screen_copy_bitmap_scaled of "src" for case "sc": every pixel inside the
 * clip, dst and the scaled image is the 1:1 copy's pixel at the mapped
 * source position; every other pixel keeps the background. */
static int scaled_matches(const char         *name,
                          const bitmap_t     *src,
                          const scaledcase_t *sc)
{
  static testscreen_t ref;
  static testscreen_t ts;

  box_t               img;
  box_t               draw;
  int                 x, y;
  pixelfmt_bgrx8888_t want;

  testscreen_init(&ref);
  screen_copy_bitmap(&ref.scr, 0, 0, src);

  testscreen_init(&ts);
  ts.scr.clip = sc->clip;
  if (screen_copy_bitmap_scaled(&ts.scr, &sc->dst, src, &sc->scale) != result_OK)
  {
    printf("screen: %s: copy_bitmap_scaled failed\n", name);
    return 0;
  }

  img.x0 = sc->dst.x0;
  img.y0 = sc->dst.y0;
  img.x1 = sc->dst.x0 + (SC_W * sc->scale.xmul + sc->scale.xdiv - 1) / sc->scale.xdiv;
  img.y1 = sc->dst.y0 + (SC_H * sc->scale.ymul + sc->scale.ydiv - 1) / sc->scale.ydiv;
  draw.x0 = 0;
  draw.y0 = 0;
  draw.x1 = WIDTH;
  draw.y1 = HEIGHT;
  if (!box_is_empty(&sc->clip))
    (void) box_intersection(&draw, &sc->clip, &draw);
  (void) box_intersection(&draw, &sc->dst, &draw);
  (void) box_intersection(&draw, &img, &draw);

  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x++)
    {
      if (x >= draw.x0 && x < draw.x1 && y >= draw.y0 && y < draw.y1)
        want = ref.pixels[(y - sc->dst.y0) * sc->scale.ydiv / sc->scale.ymul * WIDTH +
                          (x - sc->dst.x0) * sc->scale.xdiv / sc->scale.xmul];
      else
        want = BACKGROUND;

      if (ts.pixels[y * WIDTH + x] != want)
      {
        printf("screen: %s: scaled pixel (%d,%d) is %08x, want %08x\n",
               name, x, y, (unsigned int) ts.pixels[y * WIDTH + x],
               (unsigned int) want);
        return 0;
      }
    }

  return 1;
}

/* screen_copy_bitmap_scaled from deep and p4 sources at integer, fractional,
 * mixed and 1:1 scales, with dst and clip cutting the image short. */
static result_t test_copy_bitmap_scaled(void)
{
  static const scaledcase_t cases[] =
  {
    { { 3,  3, 1, 1 }, {  2,  3, 60, 60 }, { 0,  0,  0,  0 } }, /* 3x */
    { { 5, 13, 7, 5 }, {  1,  2, 64, 64 }, { 0,  0,  0,  0 } }, /* shrink x, stretch y */
    { { 2,  2, 2, 2 }, {  4,  4, 64, 64 }, { 0,  0,  0,  0 } }, /* 1:1 */
    { { 3,  3, 1, 1 }, {  2,  3, 10,  9 }, { 0,  0,  0,  0 } }, /* dst short */
    { { 3,  2, 1, 1 }, { -4, -5, 40, 40 }, { 0,  0,  0,  0 } }, /* off top-left */
    { { 4,  4, 1, 1 }, {  0,  0, 64, 64 }, { 10, 10, 30, 25 } }, /* clipped */
    { { 9,  9, 1, 1 }, {  0,  0, 64, 64 }, { 0,  0,  0,  0 } }  /* past screen */
  };

  static pixelfmt_rgba8888_t deepbuf[SC_W * SC_H];
  static unsigned char       p4buf[(SC_W + 1) / 2 * SC_H];

  colour_t                   pal[16];
  bitmap_t                   deep;
  bitmap_t                   p4;
  bitmap_t                   rle;
  testscreen_t              *ts;
  screen_scale_t             bad = { 1, 0, 1, 1 };
  int                        i, x, y;
  size_t                     ci;

  for (i = 0; i < 16; i++)
    pal[i] = colour_rgb((unsigned char) (i * 37),
                        (unsigned char) (255 - i * 13),
                        (unsigned char) (i * 91));

  memset(p4buf, 0, sizeof(p4buf));
  for (y = 0; y < SC_H; y++)
    for (x = 0; x < SC_W; x++)
    {
      i = (x * 3 + y * 5) & 15;
      p4buf[y * ((SC_W + 1) / 2) + (x >> 1)] |= (unsigned char) (i << ((x & 1) * 4));
      deepbuf[y * SC_W + x] = colour_rgb((unsigned char) (x * 30),
                                         (unsigned char) (y * 50),
                                         (unsigned char) (x * y * 7)).primary;
    }

  bitmap_init(&deep, SIZE2D(SC_W, SC_H), pixelfmt_rgba8888, SC_W * 4, NULL,
              deepbuf);
  bitmap_init(&p4, SIZE2D(SC_W, SC_H), pixelfmt_p4, (SC_W + 1) / 2, pal,
              p4buf);

  for (ci = 0; ci < NELEMS(cases); ci++)
    if (!scaled_matches("scaled rgba8888", &deep, &cases[ci]) ||
        !scaled_matches("scaled p4", &p4, &cases[ci]))
    {
      printf("screen: copy_bitmap_scaled case %d failed\n", (int) ci);
      bitmap_set_palette(&p4, NULL);
      return result_TEST_FAILED;
    }

  bitmap_set_palette(&p4, NULL);

  ts = malloc(sizeof(*ts));
  if (ts == NULL)
    return result_OOM;
  testscreen_init(ts);

  if (screen_copy_bitmap_scaled(&ts->scr, &cases[0].dst, &deep, &bad) != result_BAD_ARG)
  {
    printf("screen: copy_bitmap_scaled accepted a zero factor\n");
    free(ts);
    return result_TEST_FAILED;
  }

  rle = deep;
  rle.format = (pixelfmt_t) (pixelfmt_rgba8888 | pixelfmt_RLE_FLAG);
  if (screen_copy_bitmap_scaled(&ts->scr, &cases[0].dst, &rle,
                                &cases[0].scale) != result_NOT_SUPPORTED)
  {
    printf("screen: copy_bitmap_scaled accepted an RLE source\n");
    free(ts);
    return result_TEST_FAILED;
  }

  free(ts);

  return result_TEST_PASSED;
}

#undef SC_H
#undef SC_W

/* screen_copy_bitmap_scaled from a p4 source onto a p4 screen, which takes
 * the nibble remap path, gives the same pixels as from the same image in
 * rgba8888, opaque and with a transparent entry. */
static result_t test_copy_bitmap_scaled_p4_to_p4(void)
{
#define SP_W 7
#define SP_H 3
  static unsigned char       p4buf[(SP_W + 1) / 2 * SP_H];
  static pixelfmt_rgba8888_t deepbuf[SP_W * SP_H];
  static unsigned char       want[WIDTH / 2 * HEIGHT];
  static unsigned char       got[WIDTH / 2 * HEIGHT];

  colour_t                   srcpal[16];
  colour_t                   scrpal[16];
  bitmap_t                   p4src;
  bitmap_t                   deepsrc;
  box_t                      dst = { 3, 1, 60, 50 };
  screen_scale_t             scale = { 5, 3, 2, 1 };
  int                        i, x, y;
  int                        pass;
  screen_t                   wantscr;
  screen_t                   gotscr;

  for (i = 0; i < 16; i++)
  {
    srcpal[i] = colour_rgb((unsigned char) (i * 37),
                           (unsigned char) (255 - i * 13),
                           (unsigned char) (i * 91));
    scrpal[i] = colour_rgb((unsigned char) ((i & 1) ? 255 : 0),
                           (unsigned char) ((i & 2) ? 255 : 0),
                           (unsigned char) ((i & 4) ? ((i & 8) ? 255 : 128) : 0));
  }
  memset(p4buf, 0, sizeof(p4buf));
  for (y = 0; y < SP_H; y++)
    for (x = 0; x < SP_W; x++)
    {
      i = (x + y * SP_W) & 15;
      p4buf[y * ((SP_W + 1) / 2) + (x >> 1)] |= (unsigned char) (i << ((x & 1) * 4));
      deepbuf[y * SP_W + x] = srcpal[i].primary;
    }

  bitmap_init(&p4src, SIZE2D(SP_W, SP_H), pixelfmt_p4, (SP_W + 1) / 2, srcpal,
              p4buf);
  bitmap_init(&deepsrc, SIZE2D(SP_W, SP_H), pixelfmt_rgba8888, SP_W * 4, NULL,
              deepbuf);

  for (pass = 0; pass < 2; pass++)
  {
    if (pass == 1)
    {
      srcpal[5] = colour_rgba(0x00, 0xFF, 0x00, 0x00);
      for (i = 0; i < SP_W * SP_H; i++)
        if ((i & 15) == 5)
          deepbuf[i] = srcpal[5].primary;
      bitmap_set_palette(&p4src, srcpal);
    }

    memset(want, 0x9A, sizeof(want));
    memset(got, 0x9A, sizeof(got));
    screen_init(&wantscr, SIZE2D(WIDTH, HEIGHT), pixelfmt_p4, WIDTH / 2,
                scrpal, want);
    screen_init(&gotscr, SIZE2D(WIDTH, HEIGHT), pixelfmt_p4, WIDTH / 2,
                scrpal, got);

    if (screen_copy_bitmap_scaled(&wantscr, &dst, &deepsrc, &scale) != result_OK ||
        screen_copy_bitmap_scaled(&gotscr, &dst, &p4src, &scale) != result_OK ||
        memcmp(want, got, sizeof(want)) != 0)
    {
      printf("screen: scaled p4 to p4 differs from rgba8888, pass %d\n", pass);
      bitmap_set_palette(&p4src, NULL);
      return result_TEST_FAILED;
    }
  }

  bitmap_set_palette(&p4src, NULL);

  return result_TEST_PASSED;
#undef SP_H
#undef SP_W
}

/* screen_copy_bitmap_scaled_dithered of a flat mid-grey onto a p2 screen at
 * 3x: the dither is phased to source pixels, so every 3x3 block is uniform,
 * yet the blocks stipple where a plain scaled copy is flat. */
static result_t test_copy_bitmap_scaled_dithered(void)
{
#define SD_ROWBYTES (WIDTH / 4)
  static unsigned char       got[SD_ROWBYTES * HEIGHT];
  static unsigned char       plain[SD_ROWBYTES * HEIGHT];
  static pixelfmt_rgba8888_t srcbuf[16 * 8];

  colour_t                   pal[4];
  bitmap_t                   src;
  box_t                      dst = { 0, 0, 48, 24 };
  screen_scale_t             scale = { 3, 3, 1, 1 };
  screen_t                   scr;
  int                        x, y;

  pal[0] = colour_rgb(0x00, 0x00, 0x00);
  pal[1] = colour_rgb(0x55, 0x55, 0x55);
  pal[2] = colour_rgb(0xAA, 0xAA, 0xAA);
  pal[3] = colour_rgb(0xFF, 0xFF, 0xFF);

  for (y = 0; y < 8; y++)
    for (x = 0; x < 16; x++)
      srcbuf[y * 16 + x] = colour_rgb(0x80, 0x80, 0x80).primary;

  bitmap_init(&src, SIZE2D(16, 8), pixelfmt_rgba8888,
              16 * (int) sizeof(srcbuf[0]), NULL, srcbuf);

  memset(got, 0, sizeof(got));
  screen_init(&scr, SIZE2D(WIDTH, HEIGHT), pixelfmt_p2, SD_ROWBYTES, pal, got);
  if (screen_copy_bitmap_scaled_dithered(&scr, &dst, &src, &scale,
                                         screen_DITHER_BAYER) != result_OK)
  {
    printf("screen: scaled_dithered copy failed\n");
    return result_TEST_FAILED;
  }

  for (y = 0; y < 24; y++)
    for (x = 0; x < 48; x++)
      if (p2_pixel_at(got + y * SD_ROWBYTES, x) !=
          p2_pixel_at(got + (y / 3 * 3) * SD_ROWBYTES, x / 3 * 3))
      {
        printf("screen: scaled_dithered block at (%d,%d) is not uniform\n",
               x / 3, y / 3);
        return result_TEST_FAILED;
      }

  memset(plain, 0, sizeof(plain));
  screen_init(&scr, SIZE2D(WIDTH, HEIGHT), pixelfmt_p2, SD_ROWBYTES, pal, plain);
  screen_copy_bitmap_scaled(&scr, &dst, &src, &scale);

  if (memcmp(got, plain, sizeof(got)) == 0)
  {
    printf("screen: scaled_dithered matches the plain scaled copy\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
#undef SD_ROWBYTES
}

/* Index of the palette entry drawn at (x, y) on a p4 or p8 screen. */
static int exact_index_at(const unsigned char *base,
                          int                  rowbytes,
                          int                  log2bpp,
                          int                  x,
                          int                  y)
{
  const unsigned char *row;

  row = base + y * rowbytes;
  if (log2bpp == 3)
    return row[x];

  return (row[x >> 1] >> ((x & 1) * 4)) & 0xF;
}

/* A source whose every pixel is exactly a palette entry has nothing to dither:
 * each pixel must land on that entry whatever its threshold, on p4 and on p8,
 * from a deep source and from a paletted one. Uses the uneven PICO-8 palette,
 * where one gap across the palette is too wide for its closest colours. */
static result_t test_copy_bitmap_dithered_exact(void)
{
#define EX_SIZE 64
  static const unsigned char rgb[16][3] =
  {
    { 0x00, 0x00, 0x00 }, { 0x1D, 0x2B, 0x53 }, { 0x7E, 0x25, 0x53 },
    { 0x00, 0x87, 0x51 }, { 0xAB, 0x52, 0x36 }, { 0x5F, 0x57, 0x4F },
    { 0xC2, 0xC3, 0xC7 }, { 0xFF, 0xF1, 0xE8 }, { 0xFF, 0x00, 0x4D },
    { 0xFF, 0xA3, 0x00 }, { 0xFF, 0xEC, 0x27 }, { 0x00, 0xE4, 0x36 },
    { 0x29, 0xAD, 0xFF }, { 0x83, 0x76, 0x9C }, { 0xFF, 0x77, 0xA8 },
    { 0xFF, 0xCC, 0xAA }
  };
  static const screen_dither_t methods[] =
  {
    screen_DITHER_BAYER,
    screen_DITHER_BLUE_NOISE
  };
  static pixelfmt_rgba8888_t deep[EX_SIZE * EX_SIZE];
  static unsigned char       index8[EX_SIZE * EX_SIZE];
  static unsigned char       dst[EX_SIZE * EX_SIZE];

  colour_t                   pal[256];
  bitmap_t                   src;
  screen_t                   scr;
  int                        depth;
  int                        source;
  int                        m;
  int                        x, y;
  int                        i;
  int                        nused;
  int                        wrong;
  int                        failed;
  colour_t                   got;

  /* a desktop's palette: 16 UI colours, then a 6x6x6 cube */
  for (i = 0; i < 16; i++)
    pal[i] = colour_rgb(rgb[i][0], rgb[i][1], rgb[i][2]);
  for (i = 16; i < 16 + 216; i++)
    pal[i] = colour_rgb((i - 16) / 36 * 0x33,
                        (i - 16) / 6 % 6 * 0x33,
                        (i - 16) % 6 * 0x33);
  for (i = 16 + 216; i < 256; i++)
    pal[i] = colour_rgb(0, 0, 0);

  failed = 0;

  for (depth = 2; depth <= 3; depth++) /* log2 bpp: p4, then p8 */
    for (source = 0; source < 2; source++) /* deep, then paletted */
      for (m = 0; m < (int) NELEMS(methods); m++)
      {
        nused = depth == 2 ? 16 : 16 + 216;

        for (y = 0; y < EX_SIZE; y++)
          for (x = 0; x < EX_SIZE; x++)
          {
            i = (x + y * 5) % nused;
            deep[y * EX_SIZE + x]   = pal[i].primary;
            index8[y * EX_SIZE + x] = (unsigned char) i;
          }

        if (source == 0)
          bitmap_init(&src, SIZE2D(EX_SIZE, EX_SIZE), pixelfmt_rgba8888,
                      EX_SIZE * (int) sizeof(deep[0]), NULL, deep);
        else
          bitmap_init(&src, SIZE2D(EX_SIZE, EX_SIZE), pixelfmt_p8, EX_SIZE,
                      pal, index8);

        memset(dst, 0, sizeof(dst));
        screen_init(&scr, SIZE2D(EX_SIZE, EX_SIZE),
                    depth == 2 ? pixelfmt_p4 : pixelfmt_p8,
                    depth == 2 ? EX_SIZE / 2 : EX_SIZE, pal, dst);

        if (screen_copy_bitmap_dithered(&scr, 0, 0, &src, methods[m]) != result_OK)
        {
          printf("screen: exact dithered copy failed\n");
          return result_TEST_FAILED;
        }

        wrong = 0;
        for (y = 0; y < EX_SIZE; y++)
          for (x = 0; x < EX_SIZE; x++)
          {
            got = pal[exact_index_at(dst, depth == 2 ? EX_SIZE / 2 : EX_SIZE,
                                     depth, x, y)];
            if (got.primary != pal[(x + y * 5) % nused].primary)
              wrong++;
          }

        if (wrong != 0)
        {
          printf("screen: exact colours dithered: %s, %s source, method %d: "
                 "%d of %d pixels moved\n",
                 depth == 2 ? "p4" : "p8", source == 0 ? "deep" : "paletted", m,
                 wrong, EX_SIZE * EX_SIZE);
          failed = 1;
        }
      }

  return failed ? result_TEST_FAILED : result_TEST_PASSED;
#undef EX_SIZE
}

/* Expand a "bits"-wide channel value to 8 bits, as a 16bpp source is read. */
static unsigned int exact16_expand(unsigned int v, int bits)
{
  return bits == 5 ? (v << 3) | (v >> 2) : (v << 2) | (v >> 4);
}

/* As test_copy_bitmap_dithered_exact for 16bpp screens: every colour the
 * screen can show must survive a dithered copy unchanged, from a source of the
 * screen's format and from a 32bpp one. A flat colour between two of its steps
 * must still stipple. */
static result_t test_copy_bitmap_dithered_exact_16(void)
{
#define EX16_SIZE 256
  static const struct
  {
    const char *name;
    pixelfmt_t  format;
    int         gbits;
    int         rshift, gshift, bshift;
    unsigned    mask;
  }
  formats[] =
  {
    { "rgb565",   pixelfmt_rgb565,   6, 0, 5, 11, 0xFFFF },
    { "rgbx5551", pixelfmt_rgbx5551, 5, 0, 5, 10, 0x7FFF }
  };
  static const screen_dither_t methods[] =
  {
    screen_DITHER_BAYER,
    screen_DITHER_BLUE_NOISE
  };
  static pixelfmt_any16_t    src16[EX16_SIZE * EX16_SIZE];
  static pixelfmt_rgba8888_t src32[EX16_SIZE * EX16_SIZE];
  static pixelfmt_any16_t    dst[EX16_SIZE * EX16_SIZE];

  bitmap_t                   src;
  screen_t                   scr;
  int                        f;
  int                        source;
  int                        m;
  int                        i;
  unsigned int               v;
  int                        wrong;
  long                       prev;
  long                       sum;
  int                        varied;
  int                        failed;

  failed = 0;

  for (f = 0; f < (int) NELEMS(formats); f++)
    for (m = 0; m < (int) NELEMS(methods); m++)
    {
      for (i = 0; i < EX16_SIZE * EX16_SIZE; i++)
      {
        v        = (unsigned int) i & formats[f].mask;
        src16[i] = (pixelfmt_any16_t) v;
        src32[i] = colour_rgb(
                     (int) exact16_expand((v >> formats[f].rshift) & 31, 5),
                     (int) exact16_expand((v >> formats[f].gshift) &
                                          ((1u << formats[f].gbits) - 1),
                                          formats[f].gbits),
                     (int) exact16_expand((v >> formats[f].bshift) & 31, 5))
                   .primary;
      }

      for (source = 0; source < 2; source++) /* 32bpp, then 16bpp */
      {
        if (source == 0)
          bitmap_init(&src, SIZE2D(EX16_SIZE, EX16_SIZE), pixelfmt_rgba8888,
                      EX16_SIZE * (int) sizeof(src32[0]), NULL, src32);
        else
          bitmap_init(&src, SIZE2D(EX16_SIZE, EX16_SIZE), formats[f].format,
                      EX16_SIZE * (int) sizeof(src16[0]), NULL, src16);

        memset(dst, 0, sizeof(dst));
        screen_init(&scr, SIZE2D(EX16_SIZE, EX16_SIZE), formats[f].format,
                    EX16_SIZE * (int) sizeof(dst[0]), NULL, dst);

        if (screen_copy_bitmap_dithered(&scr, 0, 0, &src, methods[m]) != result_OK)
        {
          printf("screen: exact 16bpp dithered copy failed\n");
          return result_TEST_FAILED;
        }

        wrong = 0;
        for (i = 0; i < EX16_SIZE * EX16_SIZE; i++)
          if ((dst[i] & formats[f].mask) != (src16[i] & formats[f].mask))
            wrong++;

        if (wrong != 0)
        {
          printf("screen: exact colours dithered: %s, %s source, method %d: "
                 "%d of %d pixels moved\n",
                 formats[f].name, source == 0 ? "32bpp" : "16bpp", m, wrong,
                 EX16_SIZE * EX16_SIZE);
          failed = 1;
        }
      }

      /* a ramp must not get darker as it gets brighter: each channel's average
       * level over the threshold map never falls as the source value rises */
      for (v = 0, prev = 0; v < 256; v++)
      {
        for (i = 0; i < 64 * 64; i++)
          src32[i] = colour_rgb((int) v, (int) v, (int) v).primary;

        bitmap_init(&src, SIZE2D(64, 64), pixelfmt_rgba8888,
                    64 * (int) sizeof(src32[0]), NULL, src32);
        screen_init(&scr, SIZE2D(64, 64), formats[f].format,
                    64 * (int) sizeof(dst[0]), NULL, dst);
        screen_copy_bitmap_dithered(&scr, 0, 0, &src, methods[m]);

        sum = 0;
        for (i = 0; i < 64 * 64; i++)
          sum += (dst[i] >> formats[f].rshift) & 31;

        if (sum < prev)
        {
          printf("screen: %s, method %d: ramp falls at value %u, %ld to %ld\n",
                 formats[f].name, m, v, prev, sum);
          failed = 1;
        }
        prev = sum;
      }

      /* 0x85 is between two steps of a channel of either width */
      for (i = 0; i < EX16_SIZE * EX16_SIZE; i++)
        src32[i] = colour_rgb(0x85, 0x85, 0x85).primary;

      bitmap_init(&src, SIZE2D(EX16_SIZE, EX16_SIZE), pixelfmt_rgba8888,
                  EX16_SIZE * (int) sizeof(src32[0]), NULL, src32);
      memset(dst, 0, sizeof(dst));
      screen_init(&scr, SIZE2D(EX16_SIZE, EX16_SIZE), formats[f].format,
                  EX16_SIZE * (int) sizeof(dst[0]), NULL, dst);
      screen_copy_bitmap_dithered(&scr, 0, 0, &src, methods[m]);

      varied = 0;
      for (i = 1; i < EX16_SIZE * EX16_SIZE; i++)
        if ((dst[i] & formats[f].mask) != (dst[0] & formats[f].mask))
          varied = 1;

      if (!varied)
      {
        printf("screen: %s, method %d: off-grid colour did not stipple\n",
               formats[f].name, m);
        failed = 1;
      }
    }

  return failed ? result_TEST_FAILED : result_TEST_PASSED;
#undef EX16_SIZE
}

/* ----------------------------------------------------------------------- */

/* Read the "bpp_bits"-deep pixel at column "px" of a byte buffer using the
 * house within-byte order: p1/p2 MSB-first (bit 7 leftmost), p4 LSB-first --
 * matching screen_set_pixel_p1/p2/p4. The source block is laid down with
 * screen_set_pixel, so a wrong-endian copy shows up here as a shuffled row. */
static int house_pixel_at(const unsigned char *buf, int px, int bpp_bits)
{
  int ppb   = 8 / bpp_bits;
  int mask  = (1 << bpp_bits) - 1;
  int shift = (px % ppb) * bpp_bits;

  if (bpp_bits != 4)
    shift = 8 - bpp_bits - shift; /* MSB-first for p1/p2 */

  return (buf[px / ppb] >> shift) & mask;
}

/* screen_copy_rect on the sub-byte paletted formats (p1/p2/p4): lay down an
 * 8x8 block with screen_set_pixel, copy it right by an odd (non-byte-aligned)
 * offset, and check the pixels landed at the destination in the right order,
 * did not smear past it, and left the row below the source clear. Regression:
 * these formats used to fall through to result_NOT_SUPPORTED (full repaint on
 * every low-bpp window move); the first packed implementation then copied
 * p1/p2 with the wrong within-byte order and shuffled the row. */
static result_t test_copy_rect_packed(void)
{
  static const struct { pixelfmt_t fmt; int bits; } cases[] =
  {
    { pixelfmt_p1, 1 },
    { pixelfmt_p2, 2 },
    { pixelfmt_p4, 4 },
  };

  static unsigned char buf[WIDTH / 2 * HEIGHT]; /* widest sub-byte case */

  colour_t             pal[16];
  size_t               ci;
  int                  i;

  for (i = 0; i < 16; i++)
    pal[i] = colour_rgb((unsigned char) (i * 17),
                        (unsigned char) (i * 17),
                        (unsigned char) (i * 17));

  for (ci = 0; ci < NELEMS(cases); ci++)
  {
    screen_t scr;
    box_t    src;
    box_t    got;
    int      rowbytes, fill, x, y;

    rowbytes = WIDTH * cases[ci].bits / 8;
    fill     = (1 << cases[ci].bits) - 1; /* max index for this depth */

    memset(buf, 0, sizeof(buf));
    screen_init(&scr, SIZE2D(WIDTH, HEIGHT), cases[ci].fmt, rowbytes, pal, buf);

    /* an 8-wide, 8-tall block at the top-left, laid down the house way. Each
     * column carries a distinct non-zero index (1 + x % fill; adjacent
     * columns -- including byte-mates -- differ) so a within-byte reordering
     * can't hide behind a uniform fill. p1 has only index 1 available, so its
     * shuffle is caught by the no-smear check below instead. */
    for (y = 0; y < 8; y++)
      for (x = 0; x < 8; x++)
        screen_set_pixel(&scr, x, y, pal[1 + x % fill]);

    src = (box_t) { 0, 0, 8, 8 };
    if (screen_copy_rect(&scr, &src, POINT(5, 0), &got) != result_OK)
    {
      printf("screen: copy_rect declined a p%d screen\n", cases[ci].bits);
      return result_TEST_FAILED;
    }

    /* moved: column c of the source now sits at c+5, same index */
    for (x = 0; x < 8; x++)
      if (house_pixel_at(buf, x + 5, cases[ci].bits) != 1 + x % fill)
      {
        printf("screen: copy_rect p%d wrong pixel at x=%d "
               "(got %d want %d)\n", cases[ci].bits, x + 5,
               house_pixel_at(buf, x + 5, cases[ci].bits), 1 + x % fill);
        return result_TEST_FAILED;
      }

    /* read-before-write: column 13, just past the destination, is untouched */
    if (house_pixel_at(buf, 13, cases[ci].bits) != 0)
    {
      printf("screen: copy_rect p%d smeared past its destination\n",
             cases[ci].bits);
      return result_TEST_FAILED;
    }

    /* row 8, the first row below the 8-tall source, stayed clear */
    for (x = 0; x < 16; x++)
      if (house_pixel_at(buf + (size_t) 8 * rowbytes, x, cases[ci].bits) != 0)
      {
        printf("screen: copy_rect p%d touched a row below the source\n",
               cases[ci].bits);
        return result_TEST_FAILED;
      }
  }

  return result_TEST_PASSED;
}

/* screen_copy_rect on 1/2/4bpp against a pixel-by-pixel reference: random
 * contents, random source boxes and shifts (every phase, overlapping or not,
 * byte-aligned or not), compared over the whole screen so any stray write
 * outside the destination shows up too. */
static result_t test_copy_rect_packed_random(void)
{
  static const struct { pixelfmt_t fmt; int bits; } cases[] =
  {
    { pixelfmt_p1, 1 },
    { pixelfmt_p2, 2 },
    { pixelfmt_p4, 4 },
  };

  static unsigned char buf[WIDTH / 2 * HEIGHT];
  static unsigned char want[WIDTH * HEIGHT];
  static unsigned char before[WIDTH * HEIGHT];

  colour_t             pal[16];
  size_t               ci;
  int                  i, trial;
  unsigned int         seed;

  for (i = 0; i < 16; i++)
    pal[i] = colour_rgb((unsigned char) (i * 17),
                        (unsigned char) (i * 17),
                        (unsigned char) (i * 17));

  seed = 12345;
  for (ci = 0; ci < NELEMS(cases); ci++)
  {
    int bits, rowbytes;

    bits     = cases[ci].bits;
    rowbytes = WIDTH * bits / 8;

    for (trial = 0; trial < 400; trial++)
    {
      screen_t scr;
      box_t    src, got;
      int      w, h, dx, dy, x, y, nx, ny;

      for (i = 0; i < rowbytes * HEIGHT; i++)
      {
        seed   = seed * 1664525u + 1013904223u;
        buf[i] = (unsigned char) (seed >> 24);
      }
      screen_init(&scr, SIZE2D(WIDTH, HEIGHT), cases[ci].fmt, rowbytes, pal,
                  buf);

      for (y = 0; y < HEIGHT; y++)
        for (x = 0; x < WIDTH; x++)
          before[y * WIDTH + x] = want[y * WIDTH + x] =
            (unsigned char) house_pixel_at(buf + (size_t) y * rowbytes, x,
                                           bits);

      seed = seed * 1664525u + 1013904223u;
      w    = 1 + (int) ((seed >> 16) % 40);
      seed = seed * 1664525u + 1013904223u;
      h    = 1 + (int) ((seed >> 16) % 6);
      seed = seed * 1664525u + 1013904223u;
      src.x0 = (int) ((seed >> 16) % (unsigned) (WIDTH - w + 1));
      seed = seed * 1664525u + 1013904223u;
      src.y0 = (int) ((seed >> 16) % (unsigned) (HEIGHT - h + 1));
      src.x1 = src.x0 + w;
      src.y1 = src.y0 + h;
      seed = seed * 1664525u + 1013904223u;
      nx   = (int) ((seed >> 16) % (unsigned) (WIDTH - w + 1));
      seed = seed * 1664525u + 1013904223u;
      ny   = (int) ((seed >> 16) % (unsigned) (HEIGHT - h + 1));
      dx   = nx - src.x0;
      dy   = ny - src.y0;

      for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
          want[(ny + y) * WIDTH + nx + x] =
            before[(src.y0 + y) * WIDTH + src.x0 + x];

      if (screen_copy_rect(&scr, &src, POINT(nx, ny), &got) != result_OK)
      {
        printf("screen: copy_rect declined a p%d screen\n", bits);
        return result_TEST_FAILED;
      }

      for (y = 0; y < HEIGHT; y++)
        for (x = 0; x < WIDTH; x++)
          if (house_pixel_at(buf + (size_t) y * rowbytes, x, bits) !=
              want[y * WIDTH + x])
          {
            printf("screen: copy_rect p%d (%d,%d)+%dx%d by (%d,%d) wrong "
                   "at (%d,%d)\n", bits, src.x0, src.y0, w, h, dx, dy, x, y);
            return result_TEST_FAILED;
          }
    }
  }

  return result_TEST_PASSED;
}

/* screen_set_pixel and screen_set_pixel_value, plotted at every point from
 * just off each screen edge, must touch exactly the pixels inside both the
 * screen and the clip; an empty clip means unclipped. */
static result_t test_set_pixel_clip(void)
{
  static const box_t clips[] =
  {
    { 0, 0, 0, 0 },      /* empty: unclipped */
    { 10, 20, 30, 40 },  /* inside the screen */
    { 50, -5, 80, 10 },  /* hanging off the top-right corner */
    { 70, 70, 90, 90 }   /* wholly off screen */
  };

  static testscreen_t ts;

  size_t              ci;
  int                 fn;
  int                 x, y;
  int                 inside;

  for (ci = 0; ci < NELEMS(clips); ci++)
    for (fn = 0; fn < 2; fn++)
    {
      testscreen_init(&ts);
      ts.scr.clip = clips[ci];

      for (y = -2; y < HEIGHT + 2; y++)
        for (x = -2; x < WIDTH + 2; x++)
          if (fn == 0)
            screen_set_pixel(&ts.scr, x, y, colour_rgb(255, 255, 255));
          else
            screen_set_pixel_value(&ts.scr, x, y, 0xFFFFFFFF);

      for (y = 0; y < HEIGHT; y++)
        for (x = 0; x < WIDTH; x++)
        {
          inside = box_is_empty(&clips[ci]) ||
                   box_contains_point(&clips[ci], x, y);
          if ((ts.pixels[y * WIDTH + x] != BACKGROUND) != inside)
          {
            printf("screen: %s clip %d wrong at (%d,%d)\n",
                   fn == 0 ? "set_pixel" : "set_pixel_value", (int) ci, x, y);
            return result_TEST_FAILED;
          }
        }
    }

  return result_TEST_PASSED;
}

/* span_p4.fill on packed nibbles: every start parity and length over a short
 * row must set exactly [first, first + length) and leave the rest alone. */
static result_t test_fill_span_p4(void)
{
  unsigned char buf[16]; /* 32 pixels */
  int           first, length, x, want;

  for (first = 0; first < 8; first++)
    for (length = 0; length <= 20; length++)
    {
      memset(buf, 0x33, sizeof(buf)); /* background index 3 */
      span_p4.fill(buf, first, 0xC, length);

      for (x = 0; x < 32; x++)
      {
        want = (x >= first && x < first + length) ? 0xC : 0x3;
        if (house_pixel_at(buf, x, 4) != want)
        {
          printf("screen: span_p4 fill first=%d length=%d wrong at x=%d\n",
                 first, length, x);
          return result_TEST_FAILED;
        }
      }
    }

  return result_TEST_PASSED;
}

/* set_pixel round-trips full-precision R/G/B through the 5/6/5 or 5/5/5
 * quantisation, and copy_bitmap alpha-blends an rgba8888 source correctly,
 * for both 16bpp destination formats. */
static result_t test_16bpp_screen(pixelfmt_t fmt)
{
  static const int W = 8, H = 8;

  pixelfmt_any16_t    screenbuf[8 * 8];
  screen_t            scr;
  pixelfmt_rgba8888_t srcbuf[2 * 2];
  bitmap_t            src;
  result_t            rc;
  pixelfmt_any16_t    got;
  colour_t            c;

  screen_init(&scr, SIZE2D(W, H), fmt, W * (int) sizeof(screenbuf[0]),
             NULL, screenbuf);

  /* pure red round-trips to the format's own full-intensity red field,
   * regardless of whether that field is 5 or 6 bits wide */
  screen_set_pixel(&scr, 0, 0, colour_rgb(255, 0, 0));
  got = screenbuf[0];
  if (fmt == pixelfmt_rgb565  && got != (PIXELFMT_Rxx565_MASK >> 0)   ||
      fmt == pixelfmt_rgbx5551 && got != (PIXELFMT_Rxxx5551_MASK >> 0))
  {
    printf("screen: 16bpp set_pixel red wrong for fmt=%d: got 0x%04x\n",
           (int) fmt, got);
    return result_TEST_FAILED;
  }

  /* opaque source pixel fully overwrites the destination */
  memset(screenbuf, 0, sizeof(screenbuf));
  bitmap_init(&src, SIZE2D(2, 2), pixelfmt_rgba8888,
             2 * (int) sizeof(srcbuf[0]), NULL, srcbuf);
  srcbuf[0] = PIXELFMT_MAKE_RGBA8888(0, 0, 255, 255); /* blue, opaque */
  srcbuf[1] = PIXELFMT_MAKE_RGBA8888(0, 0, 255, 0);   /* blue, transparent */
  srcbuf[2] = srcbuf[0];
  srcbuf[3] = srcbuf[0];

  rc = screen_copy_bitmap(&scr, 0, 0, &src);
  if (rc != result_OK)
  {
    printf("screen: 16bpp copy_bitmap fmt=%d returned %d\n", (int) fmt, rc);
    return result_TEST_FAILED;
  }

  c.primary = colour_rgb(0, 0, 255).primary;
  if (screenbuf[0] != (pixelfmt_any16_t) colour_to_pixel(NULL, 0, c, fmt))
  {
    printf("screen: 16bpp copy_bitmap fmt=%d opaque pixel wrong\n", (int) fmt);
    return result_TEST_FAILED;
  }

  /* fully transparent source pixel leaves the destination untouched */
  if (screenbuf[1] != 0)
  {
    printf("screen: 16bpp copy_bitmap fmt=%d transparent pixel wrong\n",
           (int) fmt);
    return result_TEST_FAILED;
  }

  /* a 16bpp source copies opaque onto a 16bpp and a 32bpp screen */
  {
    pixelfmt_any16_t    srcpx[2 * 2];
    pixelfmt_bgrx8888_t dst32[2 * 2];
    screen_t            scr32;
    bitmap_t            src16;
    pixelfmt_any16_t    blue16;

    blue16 = (pixelfmt_any16_t) colour_to_pixel(NULL, 0, colour_rgb(0, 0, 255), fmt);
    srcpx[0] = srcpx[1] = srcpx[2] = srcpx[3] = blue16;
    bitmap_init(&src16, SIZE2D(2, 2), fmt, 2 * (int) sizeof(srcpx[0]),
                NULL, srcpx);

    memset(screenbuf, 0, sizeof(screenbuf));
    memset(dst32, 0, sizeof(dst32));
    screen_init(&scr32, SIZE2D(2, 2), pixelfmt_bgrx8888,
                2 * (int) sizeof(dst32[0]), NULL, dst32);

    if (screen_copy_bitmap(&scr, 0, 0, &src16) != result_OK ||
        screen_copy_bitmap(&scr32, 0, 0, &src16) != result_OK ||
        screenbuf[0] != blue16 ||
        dst32[0] != 0xFF0000FFu) /* opaque blue */
    {
      printf("screen: 16bpp source copy_bitmap fmt=%d wrong: 0x%04x 0x%08x\n",
             (int) fmt, screenbuf[0], (unsigned int) dst32[0]);
      return result_TEST_FAILED;
    }
  }

  /* fill_pattern: GREY50 puts fg where (x ^ y) is even, bg elsewhere; the
   * stencil variant leaves the bg pixels alone. Same via the bitmap API. */
  {
    pattern_t        pat;
    box_t            area = { 1, 1, 5, 5 };
    pixelfmt_any16_t fg, bg;
    bitmap_t         bm;
    int              pass;

    fg  = (pixelfmt_any16_t) colour_to_pixel(NULL, 0, colour_rgb(255, 0, 0), fmt);
    bg  = (pixelfmt_any16_t) colour_to_pixel(NULL, 0, colour_rgb(0, 0, 255), fmt);
    pat = pattern_from_preset(screen_PATTERN_GREY50, colour_rgb(255, 0, 0),
                              colour_rgb(0, 0, 255));

    bitmap_init(&bm, SIZE2D(W, H), fmt, W * (int) sizeof(screenbuf[0]),
                NULL, screenbuf);

    for (pass = 0; pass < 4; pass++)
    {
      int stencil = pass & 1;
      int x, y, want;

      pat.flags = stencil ? pattern_FLAG_STENCIL : 0;
      memset(screenbuf, 0, sizeof(screenbuf));
      if (pass < 2)
        screen_fill_pattern(&scr, &area, &pat);
      else if (bitmap_fill_pattern(&bm, &area, &pat) != result_OK)
        return result_TEST_FAILED;

      for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
        {
          want = 0;
          if (box_contains_point(&area, x, y))
            want = (((x ^ y) & 1) == 0) ? fg : (stencil ? 0 : bg);
          if (screenbuf[y * W + x] != want)
          {
            printf("screen: 16bpp fill_pattern fmt=%d pass=%d wrong at "
                   "(%d,%d)\n", (int) fmt, pass, x, y);
            return result_TEST_FAILED;
          }
        }
    }
  }

  /* dithering: a flat grey between two 5-bit levels (136, midway between level
   * 16's 132 and level 17's 140) must use both of them */
  {
    static const screen_dither_t methods[] =
    {
      screen_DITHER_BAYER, screen_DITHER_BLUE_NOISE
    };

    pixelfmt_rgba8888_t flat[16 * 16];
    pixelfmt_any16_t    big[16 * 16];
    screen_t            bscr;
    bitmap_t            fsrc;
    unsigned int        b;
    int                 m, i, nlo, nhi;

    for (i = 0; i < 16 * 16; i++)
      flat[i] = PIXELFMT_MAKE_RGBA8888(136, 136, 136, 255);
    bitmap_init(&fsrc, SIZE2D(16, 16), pixelfmt_rgba8888,
                16 * (int) sizeof(flat[0]), NULL, flat);
    screen_init(&bscr, SIZE2D(16, 16), fmt, 16 * (int) sizeof(big[0]),
                NULL, big);

    for (m = 0; m < 2; m++)
    {
      memset(big, 0, sizeof(big));
      if (screen_copy_bitmap_dithered(&bscr, 0, 0, &fsrc, methods[m]) !=
          result_OK)
        return result_TEST_FAILED;

      nlo = nhi = 0;
      for (i = 0; i < 16 * 16; i++)
      {
        /* blue is 5 bits in both formats */
        b = (fmt == pixelfmt_rgb565) ? PIXELFMT_xxB565(big[i])
                                     : PIXELFMT_xxBx5551(big[i]);
        if (b == 16)
          nlo++;
        else if (b == 17)
          nhi++;
      }
      if (nlo + nhi != 16 * 16 || nlo < 64 || nhi < 64)
      {
        printf("screen: 16bpp dither fmt=%d method=%d lo=%d hi=%d\n",
               (int) fmt, m, nlo, nhi);
        return result_TEST_FAILED;
      }
    }
  }

  return result_TEST_PASSED;
}

static result_t test_rgb565(void)
{
  return test_16bpp_screen(pixelfmt_rgb565);
}

static result_t test_rgbx5551(void)
{
  return test_16bpp_screen(pixelfmt_rgbx5551);
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
    test_tiled,
    test_tiled_dithered,
    test_fill_pattern,
    test_fill_checks,
    test_fill_linear_gradient,
    test_fill_linear_gradient_steps,
    test_dashed_line,
    test_draw_lines,
    test_draw_thick_line,
    test_draw_rect,
    test_draw_circle,
    test_fill_circle,
    test_rounded_rect,
    test_ellipse,
    test_copy_bitmap_p1,
    test_copy_bitmap_p2,
    test_copy_bitmap_rle_p1,
    test_copy_bitmap_rle_p2,
    test_copy_bitmap_rle_p4,
    test_copy_bitmap_rle_p8,
    test_copy_bitmap_rle_16,
    test_copy_bitmap_p8,
    test_copy_bitmap_paletted_source,
    test_copy_bitmap_p4_to_p4,
    test_copy_bitmap_bgr_source,
    test_copy_bitmap_faded,
    test_copy_bitmap_deep_formats,
    test_copy_bitmap_dithered,
    test_copy_bitmap_dithered_exact,
    test_copy_bitmap_dithered_exact_16,
    test_copy_bitmap_scaled,
    test_copy_bitmap_scaled_p4_to_p4,
    test_copy_bitmap_scaled_dithered,
    test_copy_rect_packed,
    test_copy_rect_packed_random,
    test_fill_span_p4,
    test_set_pixel_clip,
    test_rgb565,
    test_rgbx5551
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
