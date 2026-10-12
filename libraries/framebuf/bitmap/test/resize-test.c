/* framebuf/bitmap/test/resize-test.c -- bitmap_resize tests */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "base/result.h"

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

/* ----------------------------------------------------------------------- */

result_t bitmap_resize_test(const char *resources)
{
  (void) resources;

  return test_resize() ? result_TEST_PASSED : result_TEST_FAILED;
}
