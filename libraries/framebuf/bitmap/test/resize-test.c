/* framebuf/bitmap/test/resize-test.c -- bitmap_resize tests */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "base/result.h"
#include "base/utils.h"

#include "framebuf/bitmap.h"
#include "framebuf/pixelfmt.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

static void resize_free(bitmap_t *bm)
{
  free(bm->base);
  free(bm);
}

/* 2x1 bitmap, black then white (255 in every lane) */
static int test_resize(void)
{
  uint32_t  px[2] = { 0x00000000u, 0xFFFFFFFFu };
  bitmap_t  src;
  bitmap_t *out;
  uint8_t  *b;
  int       ok;

  ok = 1;
  bitmap_init(&src, SIZE2D(2, 1), pixelfmt_rgba8888, 8, NULL, px);

  /* nearest doubling: each pixel repeated */
  if (bitmap_resize(&src, SIZE2D(4, 2), bitmap_resample_NEAREST, 0, &out) !=
      result_OK)
    return 0;
  b = out->base;
  if (out->size.w != 4 || out->size.h != 2 || out->rowbytes != 16 ||
      b[0] != 0 || b[4] != 0 || b[8] != 255 || b[12] != 255 ||
      b[16] != 0 || b[28] != 255)
  {
    printf("resize: nearest wrong\n");
    ok = 0;
  }
  resize_free(out);

  /* bilinear doubling: ends clamp, middle blends monotonically */
  if (bitmap_resize(&src, SIZE2D(4, 1), bitmap_resample_BILINEAR, 0, &out) !=
      result_OK)
    return 0;
  b = out->base;
  if (b[0] != 0 || b[12] != 255 || !(b[4] > 0 && b[4] < b[8] && b[8] < 255))
  {
    printf("resize: bilinear wrong (%d %d %d %d)\n", b[0], b[4], b[8], b[12]);
    ok = 0;
  }
  resize_free(out);

  /* same size bilinear is the identity */
  if (bitmap_resize(&src, SIZE2D(2, 1), bitmap_resample_BILINEAR, 0, &out) !=
      result_OK)
    return 0;
  b = out->base;
  if (b[0] != 0 || b[4] != 255)
  {
    printf("resize: identity wrong\n");
    ok = 0;
  }
  resize_free(out);

  /* linear light: 0 and 255 averaged is ~188, not 128 */
  if (bitmap_resize(&src, SIZE2D(1, 1), bitmap_resample_BILINEAR, 1, &out) !=
      result_OK)
    return 0;
  b = out->base;
  if (b[0] < 180 || b[0] > 195 || b[3] != 128)
  {
    printf("resize: linear wrong (%d, alpha %d)\n", b[0], b[3]);
    ok = 0;
  }
  resize_free(out);

  /* box: 2x1 to 1x1 is the exact mean; 4 to 3 weights the middle pixels */
  if (bitmap_resize(&src, SIZE2D(1, 1), bitmap_resample_BOX, 0, &out) !=
      result_OK)
    return 0;
  b = out->base;
  if (b[0] != 128)
  {
    printf("resize: box wrong (%d)\n", b[0]);
    ok = 0;
  }
  resize_free(out);

  /* box enlarging falls back to bilinear */
  if (bitmap_resize(&src, SIZE2D(4, 1), bitmap_resample_BOX, 0, &out) !=
      result_OK)
    return 0;
  b = out->base;
  if (b[0] != 0 || b[12] != 255 || !(b[4] > 0 && b[4] < b[8]))
  {
    printf("resize: box enlarge wrong\n");
    ok = 0;
  }
  resize_free(out);

  /* cubic: same-size is the identity, and never leaves 0..255 */
  if (bitmap_resize(&src, SIZE2D(2, 1), bitmap_resample_CUBIC, 0, &out) !=
      result_OK)
    return 0;
  b = out->base;
  if (b[0] != 0 || b[4] != 255)
  {
    printf("resize: cubic identity wrong (%d %d)\n", b[0], b[4]);
    ok = 0;
  }
  resize_free(out);

  if (bitmap_resize(&src, SIZE2D(8, 1), bitmap_resample_CUBIC, 1, &out) !=
      result_OK)
    return 0;
  b = out->base;
  if (b[0] != 0 || b[28] != 255 || b[12] >= b[16])
  {
    printf("resize: cubic linear wrong (%d %d)\n", b[0], b[28]);
    ok = 0;
  }
  resize_free(out);

  /* errors */
  if (bitmap_resize(&src, SIZE2D(0, 1), bitmap_resample_NEAREST, 0, &out) !=
      result_BAD_ARG)
  {
    printf("resize: empty size accepted\n");
    ok = 0;
  }

  return ok;
}

/* Shrink 4:1 lines one pixel wide, spaced four apart, along either axis. The
 * result must be near their mean: filters that skip source pixels when
 * shrinking miss the lines altogether. */
static int test_resize_shrink_aliasing(void)
{
  static const bitmap_resample_t methods[] =
  {
    bitmap_resample_BILINEAR,
    bitmap_resample_BOX,
    bitmap_resample_CUBIC,
    bitmap_resample_LANCZOS3
  };

  static uint32_t px[64 * 64];
  bitmap_t        src;
  bitmap_t       *out;
  uint8_t        *b;
  int             ok;
  int             axis, m;
  int             x, y;

  ok = 1;
  bitmap_init(&src, SIZE2D(64, 64), pixelfmt_rgba8888, 64 * 4, NULL, px);

  for (axis = 0; axis < 2; axis++)
  {
    for (y = 0; y < 64; y++)
      for (x = 0; x < 64; x++)
        px[y * 64 + x] = ((axis == 0 ? x : y) % 4 == 0) ? 0xFFFFFFFFu : 0u;

    for (m = 0; m < (int) NELEMS(methods); m++)
    {
      if (bitmap_resize(&src, SIZE2D(16, 16), methods[m], 0, &out) != result_OK)
        return 0;
      b = out->base;
      if (b[8 * out->rowbytes + 8 * 4] < 58 ||
          b[8 * out->rowbytes + 8 * 4] > 70)
      {
        printf("resize: method %d axis %d aliased (%d)\n", (int) methods[m],
               axis, b[8 * out->rowbytes + 8 * 4]);
        ok = 0;
      }
      resize_free(out);
    }
  }

  return ok;
}

/* Ringing must not disturb a flat image, in either colour space. */
static int test_resize_lanczos_flat(void)
{
  static uint32_t px[32 * 32];
  bitmap_t        src;
  bitmap_t       *out;
  uint8_t        *b;
  int             ok;
  int             i, linear;

  ok = 1;
  bitmap_init(&src, SIZE2D(32, 32), pixelfmt_rgba8888, 32 * 4, NULL, px);

  for (i = 0; i < 32 * 32; i++)
    px[i] = 0xC8C8C8C8u;

  for (linear = 0; linear < 2; linear++)
  {
    if (bitmap_resize(&src, SIZE2D(7, 45), bitmap_resample_LANCZOS3, linear,
                      &out) != result_OK)
      return 0;
    b = out->base;
    for (i = 0; i < 7 * 45 * 4; i++)
    {
      if (b[i] != 0xC8)
      {
        printf("resize: lanczos flat wrong (%d at %d)\n", b[i], i);
        ok = 0;
        break;
      }
    }
    resize_free(out);
  }

  return ok;
}

/* ----------------------------------------------------------------------- */

result_t bitmap_resize_test(const char *resources)
{
  int ok;

  (void) resources;

  ok = test_resize();
  ok = test_resize_shrink_aliasing() && ok;
  ok = test_resize_lanczos_flat() && ok;

  return ok ? result_TEST_PASSED : result_TEST_FAILED;
}
