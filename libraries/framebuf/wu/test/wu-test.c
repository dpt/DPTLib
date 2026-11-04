/* framebuf/wu/test/wu-test.c */

#include <stdio.h>
#include <stdlib.h>

#include "base/result.h"

#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/octree.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/wu.h"

#include "test/all-tests.h"

#define W 64
#define H 64

static int palette_has(const colour_t     *palette,
                       int                 n,
                       pixelfmt_rgbx8888_t px)
{
  int i;

  for (i = 0; i < n; i++)
    if ((palette[i].primary & 0x00FFFFFFu) == (px & 0x00FFFFFFu))
      return 1;

  return 0;
}

/* total squared error of the pixels mapped to their nearest palette entries */
static double palette_error(const pixelfmt_rgbx8888_t *pixels,
                            const colour_t            *palette,
                            int                        n)
{
  double       total;
  int          i, j;
  int          best;
  unsigned int r, g, b;
  int          dr, dg, db;
  int          d;

  total = 0.0;
  for (i = 0; i < W * H; i++)
  {
    best = 3 * 255 * 255 + 1;
    for (j = 0; j < n; j++)
    {
      colour_get_rgb(&palette[j], &r, &g, &b);
      dr = (int) PIXELFMT_Rxxx8888(pixels[i]) - (int) r;
      dg = (int) PIXELFMT_xGxx8888(pixels[i]) - (int) g;
      db = (int) PIXELFMT_xxBx8888(pixels[i]) - (int) b;
      d  = dr * dr + dg * dg + db * db;
      if (d < best)
        best = d;
    }
    total += best;
  }

  return total;
}

result_t wu_test(const char *resources)
{
  static const pixelfmt_rgbx8888_t few[5] =
  {
    0x000000u, 0xFFFFFFu, 0x0000FFu, 0x123456u, 0x00FF00u
  };

  result_t                   rc;
  static pixelfmt_rgbx8888_t pixels[W * H];
  bitmap_t                   bm;
  size2d_t                   size = { W, H };
  colour_t                   palette[wu_MAX_COLOURS];
  int                        nfound;
  int                        x, y;
  int                        i;
  double                     wu_error, octree_error;

  (void) resources;

  rc = bitmap_init(&bm, size, pixelfmt_rgbx8888, W * 4, NULL, pixels);
  if (rc != result_OK)
    return result_TEST_FAILED;

  /* 1. colours in separate bins come back exactly */
  for (i = 0; i < W * H; i++)
    pixels[i] = few[i % 5] | 0xFF000000u;

  rc = wu_make_palette(&bm, 16, palette, &nfound);
  if (rc != result_OK || nfound != 5)
  {
    printf("wu: expected 5 colours, got %d (rc %d)\n", nfound, rc);
    return result_TEST_FAILED;
  }
  for (i = 0; i < 5; i++)
  {
    if (!palette_has(palette, nfound, few[i]))
    {
      printf("wu: colour %06x missing\n", few[i]);
      return result_TEST_FAILED;
    }
  }

  /* 2. colours in a shared bin merge to their mean */
  for (i = 0; i < W * H; i++)
    pixels[i] = (i & 1 ? 0x020202u : 0x000000u) | 0xFF000000u;

  rc = wu_make_palette(&bm, 16, palette, &nfound);
  if (rc != result_OK || nfound != 1 || !palette_has(palette, 1, 0x010101u))
  {
    printf("wu: shared bin gave %d colours (rc %d)\n", nfound, rc);
    return result_TEST_FAILED;
  }

  /* 3. a gradient spanning many bins reduces to exactly N */
  for (y = 0; y < H; y++)
    for (x = 0; x < W; x++)
      pixels[y * W + x] = PIXELFMT_MAKE_RGBA8888(x * 4, y * 4, (x + y) * 2, 255);

  for (i = wu_MIN_COLOURS; i <= wu_MAX_COLOURS; i *= 2)
  {
    rc = wu_make_palette(&bm, i, palette, &nfound);
    if (rc != result_OK || nfound != i)
    {
      printf("wu: N=%d gave %d colours (rc %d)\n", i, nfound, rc);
      return result_TEST_FAILED;
    }
  }

  /* 4. the point of it all: less error than the octree */
  for (i = 4; i <= 64; i *= 2)
  {
    rc = wu_make_palette(&bm, i, palette, &nfound);
    if (rc != result_OK)
      return result_TEST_FAILED;
    wu_error = palette_error(pixels, palette, nfound);

    rc = octree_make_palette(&bm, i, palette, &nfound);
    if (rc != result_OK)
      return result_TEST_FAILED;
    octree_error = palette_error(pixels, palette, nfound);

    if (wu_error >= octree_error)
    {
      printf("wu: N=%d error %f not below octree's %f\n", i, wu_error,
             octree_error);
      return result_TEST_FAILED;
    }
  }

  /* 5. bad arguments */
  if (wu_make_palette(&bm, 1, palette, &nfound) != result_BAD_ARG ||
      wu_make_palette(&bm, 257, palette, &nfound) != result_BAD_ARG)
  {
    printf("wu: bad palette size accepted\n");
    return result_TEST_FAILED;
  }

  bm.format = pixelfmt_bgrx8888;
  if (wu_make_palette(&bm, 16, palette, &nfound) != result_BAD_ARG)
  {
    printf("wu: bad format accepted\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}
