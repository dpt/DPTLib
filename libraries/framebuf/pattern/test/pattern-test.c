/* framebuf/pattern/test/pattern-test.c */

#include <stdio.h>
#include <stdlib.h>

#include "base/result.h"
#include "framebuf/colour.h"

#include "framebuf/pattern.h"

/* ----------------------------------------------------------------------- */

/* Number of set (fg) pixels in the tile, 0..64. */
static int coverage(const pattern_t *pat)
{
  int n;
  int y;
  int x;

  n = 0;
  for (y = 0; y < 8; y++)
    for (x = 0; x < 8; x++)
      n += (pat->bits[y] >> x) & 1;

  return n;
}

/* True if the tile's average colour is within 'tol' of (r,g,b) per channel. */
static int mixes_to(const pattern_t *pat, int r, int g, int b, int tol)
{
  int          n;
  unsigned int fr, fg, fb;
  unsigned int br, bg, bb;
  int          mr, mg, mb;

  n = coverage(pat);
  colour_get_rgb(&pat->fg, &fr, &fg, &fb);
  colour_get_rgb(&pat->bg, &br, &bg, &bb);

  mr = (int) (fr * n + br * (64 - n) + 32) / 64;
  mg = (int) (fg * n + bg * (64 - n) + 32) / 64;
  mb = (int) (fb * n + bb * (64 - n) + 32) / 64;

  return abs(mr - r) <= tol && abs(mg - g) <= tol && abs(mb - b) <= tol;
}

/* True if every pixel the tile paints is colour 'c'. */
static int solid_in(const pattern_t *pat, colour_t c)
{
  int n;

  n = coverage(pat);

  return (n == 0  || pat->fg.primary == c.primary) &&
         (n == 64 || pat->bg.primary == c.primary);
}

#define CHECK(expr)                                             \
  do                                                            \
  {                                                             \
    if (!(expr))                                                \
    {                                                           \
      printf("pattern: line %d: %s failed\n", __LINE__, #expr); \
      ok = 0;                                                   \
    }                                                           \
  }                                                             \
  while (0)

/* ----------------------------------------------------------------------- */

result_t pattern_test(const char *resources)
{
  colour_t  bw[2];
  colour_t  bwg[3];
  colour_t  rb[2];
  pattern_t pat;
  int       ok;

  (void) resources;

  bw[0] = colour_rgb(0, 0, 0);
  bw[1] = colour_rgb(255, 255, 255);

  ok = 1;

  /* black/white only: greys must stipple to roughly the right level */
  pat = pattern_from_colour(bw, 2, colour_rgb(64, 64, 64));
  CHECK(mixes_to(&pat, 64, 64, 64, 4));
  pat = pattern_from_colour(bw, 2, colour_rgb(200, 200, 200));
  CHECK(mixes_to(&pat, 200, 200, 200, 4));

  /* exact entry comes back solid */
  pat = pattern_from_colour(bw, 2, colour_rgb(255, 255, 255));
  CHECK(solid_in(&pat, bw[1]));

  /* a near grey entry beats a black/white stipple */
  bwg[0] = bw[0];
  bwg[1] = bw[1];
  bwg[2] = colour_rgb(120, 120, 120);
  pat = pattern_from_colour(bwg, 3, colour_rgb(128, 128, 128));
  CHECK(solid_in(&pat, bwg[2]));

  /* chromatic mix */
  rb[0] = colour_rgb(255, 0, 0);
  rb[1] = colour_rgb(0, 0, 255);
  pat = pattern_from_colour(rb, 2, colour_rgb(128, 0, 128));
  CHECK(mixes_to(&pat, 128, 0, 128, 4));

  return ok ? result_TEST_PASSED : result_TEST_FAILED;
}
