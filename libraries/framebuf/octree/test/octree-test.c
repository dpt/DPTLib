/* framebuf/octree/test/octree-test.c */

#include <stdio.h>
#include <stdlib.h>

#include "base/result.h"

#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/octree.h"
#include "framebuf/pixelfmt.h"

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

result_t octree_test(const char *resources)
{
  static const pixelfmt_rgbx8888_t few[5] =
  {
    0x000000u, 0xFFFFFFu, 0x0000FFu, 0x123456u, 0x00FF00u
  };

  result_t            rc;
  pixelfmt_rgbx8888_t pixels[W * H];
  bitmap_t            bm;
  size2d_t            size = { W, H };
  colour_t            palette[octree_MAX_COLOURS];
  int                 nfound;
  int                 x, y;
  int                 i;

  (void) resources;

  rc = bitmap_init(&bm, size, pixelfmt_rgbx8888, W * 4, NULL, pixels);
  if (rc != result_OK)
    return result_TEST_FAILED;

  /* 1. few distinct colours come back exactly */
  for (i = 0; i < W * H; i++)
    pixels[i] = few[i % 5] | 0xFF000000u;

  rc = octree_make_palette(&bm, 16, palette, &nfound);
  if (rc != result_OK || nfound != 5)
  {
    printf("octree: expected 5 colours, got %d (rc %d)\n", nfound, rc);
    return result_TEST_FAILED;
  }
  for (i = 0; i < 5; i++)
  {
    if (!palette_has(palette, nfound, few[i]))
    {
      printf("octree: colour %06x missing\n", few[i]);
      return result_TEST_FAILED;
    }
  }

  /* 2. a gradient of 4096 distinct colours reduces to exactly N */
  for (y = 0; y < H; y++)
    for (x = 0; x < W; x++)
      pixels[y * W + x] = PIXELFMT_MAKE_RGBA8888(x * 4, y * 4, (x + y) * 2, 255);

  for (i = octree_MIN_COLOURS; i <= octree_MAX_COLOURS; i *= 2)
  {
    rc = octree_make_palette(&bm, i, palette, &nfound);
    if (rc != result_OK || nfound != i)
    {
      printf("octree: N=%d gave %d colours (rc %d)\n", i, nfound, rc);
      return result_TEST_FAILED;
    }
  }

  /* 3. bad arguments */
  if (octree_make_palette(&bm, 1, palette, &nfound) != result_BAD_ARG ||
      octree_make_palette(&bm, 257, palette, &nfound) != result_BAD_ARG)
  {
    printf("octree: bad palette size accepted\n");
    return result_TEST_FAILED;
  }

  bm.format = pixelfmt_bgrx8888;
  if (octree_make_palette(&bm, 16, palette, &nfound) != result_BAD_ARG)
  {
    printf("octree: bad format accepted\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}
