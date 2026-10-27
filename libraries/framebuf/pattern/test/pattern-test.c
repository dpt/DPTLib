/* framebuf/pattern/test/pattern-test.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"
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

/* pattern_from_colour's error for target 't' shown as a mix of 'b' over 'a'
 * at Bayer 'level', 64x scale, spread penalty included. */
static long mix_error(colour_t t, colour_t a, colour_t b, int level)
{
  unsigned int tr, tg, tb;
  unsigned int ar, ag, ab;
  unsigned int br, bg, bb;
  long         dr, dg, db;
  long         er, eg, eb;
  long         err;

  colour_get_rgb(&t, &tr, &tg, &tb);
  colour_get_rgb(&a, &ar, &ag, &ab);
  colour_get_rgb(&b, &br, &bg, &bb);

  dr = (long) br - (long) ar;
  dg = (long) bg - (long) ag;
  db = (long) bb - (long) ab;

  er  = 64 * ((long) tr - (long) ar) - dr * level;
  eg  = 64 * ((long) tg - (long) ag) - dg * level;
  eb  = 64 * ((long) tb - (long) ab) - db * level;
  err = er * er + eg * eg + eb * eb;
  if (level > 0 && level < 64)
    err += (dr * dr + dg * dg + db * db) * 64;

  return err;
}

/* The lowest error over every pair, by brute force. Each pair mixes at the
 * level nearest the target's projection onto it; its ends are covered by the
 * solid pairs. */
static long best_error(const colour_t *palette, int n, colour_t t)
{
  unsigned int tr, tg, tb;
  long         best;
  int          i;
  unsigned int ar, ag, ab;
  int          j;
  unsigned int br, bg, bb;
  long         dr, dg, db;
  long         len2;
  long         dot;
  int          level;
  long         err;

  colour_get_rgb(&t, &tr, &tg, &tb);

  best = -1;
  for (i = 0; i < n; i++)
  {
    colour_get_rgb(&palette[i], &ar, &ag, &ab);
    for (j = i; j < n; j++)
    {
      colour_get_rgb(&palette[j], &br, &bg, &bb);
      dr    = (long) br - (long) ar;
      dg    = (long) bg - (long) ag;
      db    = (long) bb - (long) ab;
      len2  = dr * dr + dg * dg + db * db;
      dot   = ((long) tr - (long) ar) * dr + ((long) tg - (long) ag) * dg +
              ((long) tb - (long) ab) * db;
      level = 0;
      if (len2 > 0 && dot > 0)
        level = (int) MIN((dot * 64 + len2 / 2) / len2, 64);

      err = mix_error(t, palette[i], palette[j], level);
      if (best < 0 || err < best)
        best = err;
    }
  }

  return best;
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
  int       seen[64];
  int       i;
  colour_t  pal[16 + 216];
  unsigned  seed;
  colour_t  t;

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
  CHECK(pattern_is_solid(&pat));

  /* a stipple is solid only when its two colours match; an empty stencil
   * paints nothing, so is not */
  pat = pattern_from_preset(screen_PATTERN_GREY50, bw[0], bw[1]);
  CHECK(!pattern_is_solid(&pat));
  pat.bg = pat.fg;
  CHECK(pattern_is_solid(&pat));
  memset(pat.bits, 0, sizeof(pat.bits));
  CHECK(pattern_is_solid(&pat));
  pat.flags = pattern_FLAG_STENCIL;
  CHECK(!pattern_is_solid(&pat));

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

  /* the clustered-dot matrix holds each threshold once and tiles */
  memset(seen, 0, sizeof(seen));
  for (i = 0; i < 64; i++)
    seen[pattern_clustered_threshold(i & 7, i >> 3)]++;
  for (i = 0; i < 64; i++)
    CHECK(seen[i] == 1);
  CHECK(pattern_clustered_threshold(-1, -8) ==
        pattern_clustered_threshold(7, 0));

  /* the sized Bayer matrices: 8x8 matches the fixed one, smaller ones are
   * the standard 2x2 and 4x4, and all tile through negative coordinates */
  for (i = 0; i < 64; i++)
    CHECK(pattern_bayer_threshold_n(i & 7, i >> 3, 3) ==
          pattern_bayer_threshold(i & 7, i >> 3));
  CHECK(pattern_bayer_threshold_n(1, 0, 1) == 2);
  CHECK(pattern_bayer_threshold_n(0, 1, 1) == 3);
  CHECK(pattern_bayer_threshold_n(1, 1, 1) == 1);
  CHECK(pattern_bayer_threshold_n(3, 1, 2) == 6);
  CHECK(pattern_bayer_threshold_n(-1, -3, 2) ==
        pattern_bayer_threshold_n(3, 1, 2));
  CHECK(pattern_bayer_threshold_n(5, 9, 0) == 0);

  /* the pruned pair search finds as good a mix as trying every pair: a
   * 256-colour-style palette of a web-safe cube plus odd extras, against
   * pseudo-random targets */
  seed = 1;
  for (i = 0; i < 16; i++)
  {
    seed   = seed * 1103515245u + 12345u;
    pal[i] = colour_rgb((seed >> 8) & 0xFF, (seed >> 16) & 0xFF,
                        (seed >> 24) & 0xFF);
  }
  for (i = 0; i < 216; i++)
    pal[16 + i] = colour_rgb(i / 36 * 51, i / 6 % 6 * 51, i % 6 * 51);
  for (i = 0; i < 24; i++)
  {
    seed = seed * 1103515245u + 12345u;
    t    = colour_rgb((seed >> 8) & 0xFF, (seed >> 16) & 0xFF,
                      (seed >> 24) & 0xFF);
    pat  = pattern_from_colour(pal, (int) NELEMS(pal), t);
    CHECK(mix_error(t, pat.bg, pat.fg, coverage(&pat)) ==
          best_error(pal, (int) NELEMS(pal), t));
  }

  return ok ? result_TEST_PASSED : result_TEST_FAILED;
}
