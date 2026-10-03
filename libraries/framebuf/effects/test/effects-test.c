/* framebuf/effects/test/effects-test.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"

#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/effects.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/tonemap.h"

#include "test/all-tests.h"

#define W      13
#define H      9
#define STRIDE (W + 3) /* pixels per row, including padding */

#define FAIL(...) do { printf("effects: " __VA_ARGS__); printf("\n"); \
                       return result_TEST_FAILED; } while (0)

static int channel(unsigned int px, int shift)
{
  return (px >> shift) & 0xFF;
}

static result_t make(bitmap_t    *bm,
                     unsigned int *pixels,
                     pixelfmt_t    fmt)
{
  size2d_t size = { W, H };

  memset(pixels, 0, STRIDE * H * sizeof(*pixels));

  return bitmap_init(bm, size, fmt, STRIDE * 4, NULL, pixels);
}

static void fill(unsigned int *pixels, unsigned int px)
{
  int i;

  for (i = 0; i < STRIDE * H; i++)
    pixels[i] = px;
}

static result_t test_blur(void)
{
  static const int amounts[] = { 2, 6, 9, 20, 48 }; /* 1, 2, 3, 7+ stages */

  bitmap_t     src, dst, inplace;
  unsigned int a[STRIDE * H], b[STRIDE * H], c[STRIDE * H];
  int          i;
  int          x, y;

  if (make(&src, a, pixelfmt_rgba8888) != result_OK ||
      make(&dst, b, pixelfmt_rgba8888) != result_OK)
    FAIL("bad bitmap");

  /* flat colour stays flat, alpha is untouched. The kernel is fixed point
   * so the error grows with its width. */
  for (i = 0; i < 5; i++)
  {
    int tol;

    tol = 2 + amounts[i] / 3;

    fill(a, PIXELFMT_MAKE_RGBA8888(200, 100, 50, 77));

    if (effects_blur_apply(&src, &dst, effects_blur_BOX, amounts[i]) !=
        result_OK)
      FAIL("blur failed");

    for (y = 0; y < H; y++)
    {
      for (x = 0; x < W; x++)
      {
        unsigned int px = b[y * STRIDE + x];

        if (abs(channel(px, 0) - 200) > tol || abs(channel(px, 8) - 100) > tol ||
            abs(channel(px, 16) - 50) > tol || channel(px, 24) != 77)
          FAIL("amount %d: flat colour changed at %d,%d: %08x",
               amounts[i], x, y, px);
      }
    }
  }

  /* an impulse spreads out symmetrically */
  fill(a, PIXELFMT_MAKE_RGBA8888(0, 0, 0, 255));
  a[4 * STRIDE + 6] = PIXELFMT_MAKE_RGBA8888(255, 255, 255, 255);

  if (effects_blur_apply(&src, &dst, effects_blur_GAUSSIAN, 3) != result_OK)
    FAIL("blur failed");

  if (channel(b[4 * STRIDE + 6], 0) >= 255 ||
      channel(b[4 * STRIDE + 7], 0) == 0 ||
      channel(b[4 * STRIDE + 5], 0) != channel(b[4 * STRIDE + 7], 0) ||
      channel(b[3 * STRIDE + 6], 0) != channel(b[5 * STRIDE + 6], 0))
    FAIL("impulse did not spread symmetrically");

  /* in place gives the same answer */
  if (make(&inplace, c, pixelfmt_rgba8888) != result_OK)
    FAIL("bad bitmap");
  memcpy(c, a, sizeof(a));

  if (effects_blur_apply(&inplace, &inplace, effects_blur_GAUSSIAN, 3) !=
      result_OK)
    FAIL("in-place blur failed");

  for (y = 0; y < H; y++)
  {
    if (memcmp(&b[y * STRIDE], &c[y * STRIDE], W * sizeof(*b)) != 0)
      FAIL("in-place blur differs on row %d", y);
  }

  return result_TEST_PASSED;
}

static result_t test_sharpen(void)
{
  bitmap_t     src, dst;
  unsigned int a[STRIDE * H], b[STRIDE * H];
  int          x, y;

  if (make(&src, a, pixelfmt_bgra8888) != result_OK ||
      make(&dst, b, pixelfmt_bgra8888) != result_OK)
    FAIL("bad bitmap");

  fill(a, PIXELFMT_MAKE_BGRA8888(90, 120, 150, 31));

  if (effects_sharpen_apply(&src, &dst) != result_OK)
    FAIL("sharpen failed");

  for (y = 0; y < H; y++)
  {
    for (x = 0; x < W; x++)
    {
      unsigned int px = b[y * STRIDE + x];

      if (abs(channel(px, 16) - 90) > 2 || abs(channel(px, 8) - 120) > 2 ||
          abs(channel(px, 0) - 150) > 2 || channel(px, 24) != 31)
        FAIL("sharpen changed flat colour at %d,%d: %08x", x, y, px);
    }
  }

  return result_TEST_PASSED;
}

static result_t test_blend(void)
{
  bitmap_t     a, b, dst;
  unsigned int pa[STRIDE * H], pb[STRIDE * H], pd[STRIDE * H];

  if (make(&a, pa, pixelfmt_rgba8888) != result_OK ||
      make(&b, pb, pixelfmt_rgba8888) != result_OK ||
      make(&dst, pd, pixelfmt_rgba8888) != result_OK)
    FAIL("bad bitmap");

  fill(pa, PIXELFMT_MAKE_RGBA8888(0, 100, 255, 0));
  fill(pb, PIXELFMT_MAKE_RGBA8888(200, 0, 255, 255));

  effects_blend(&a, &b, &dst, 0);
  if (pd[0] != pa[0])
    FAIL("blend 0 gave %08x", pd[0]);

  effects_blend(&a, &b, &dst, 256);
  if (pd[0] != pb[0])
    FAIL("blend 256 gave %08x", pd[0]);

  effects_blend(&a, &b, &dst, 128);
  if (channel(pd[0], 0) != 100 || channel(pd[0], 8) != 50 ||
      channel(pd[0], 16) != 255 || channel(pd[0], 24) != 127)
    FAIL("blend 128 gave %08x", pd[0]);

  return result_TEST_PASSED;
}

static result_t test_grey_clear(void)
{
  bitmap_t     src, dst;
  unsigned int a[STRIDE * H], b[STRIDE * H];
  colour_t     c;

  if (make(&src, a, pixelfmt_bgra8888) != result_OK ||
      make(&dst, b, pixelfmt_bgra8888) != result_OK)
    FAIL("bad bitmap");

  /* pure red in BGRA8888 is 0xAARR0000 */
  fill(a, 0x40FF0000u);

  if (effects_grey_apply(&src, &dst) != result_OK)
    FAIL("grey failed");

  if (b[0] != 0x404C4C4Cu) /* (255 * 19595) >> 16 = 76 */
    FAIL("grey of red gave %08x", b[0]);

  fill(a, 0x40808080u);
  effects_grey_apply(&src, &dst);
  if (b[0] != a[0])
    FAIL("grey changed a grey pixel: %08x", b[0]);

  /* clear respects the format's channel order and keeps alpha */
  c = colour_rgb(0x11, 0x22, 0x33);
  fill(a, 0x40000000u);

  if (effects_clear_apply(&src, &dst, c) != result_OK)
    FAIL("clear failed");

  if (b[0] != 0x40112233u) /* A R G B */
    FAIL("clear gave %08x", b[0]);

  return result_TEST_PASSED;
}

static result_t test_levels(void)
{
  bitmap_t     src, dst;
  unsigned int a[STRIDE * H], b[STRIDE * H];
  int          i;

  if (make(&src, a, pixelfmt_rgbx8888) != result_OK ||
      make(&dst, b, pixelfmt_rgbx8888) != result_OK)
    FAIL("bad bitmap");

  /* a half-range ramp: expand stretches it to the full range */
  fill(a, 0);
  for (i = 0; i < W; i++)
    a[i] = PIXELFMT_MAKE_RGBA8888(64 + i * 10, 64 + i * 10, 64 + i * 10, 0);

  if (effects_expand_apply(&src, &dst, 0) != result_OK)
    FAIL("expand failed");

  /* row 0 has the full ramp, but rows 1.. are black, so range is 0..184 */
  if (channel(b[W - 1], 0) != 255)
    FAIL("expand did not reach white: %08x", b[W - 1]);

  /* equalise: the brightest used level maps to 255 */
  if (effects_equalise_apply(&src, &dst) != result_OK)
    FAIL("equalise failed");

  if (channel(b[W - 1], 0) != 255 || channel(b[STRIDE], 0) >
      channel(b[W - 1], 0))
    FAIL("equalise result wrong: %08x", b[W - 1]);

  return result_TEST_PASSED;
}

static result_t test_tonemap(void)
{
  bitmap_t               src, dst;
  unsigned int           a[STRIDE * H], b[STRIDE * H];
  tonemap_t             *map;
  tonemap_spec_t         spec;
  const tonemap_table_t *red;

  if (make(&src, a, pixelfmt_rgba8888) != result_OK ||
      make(&dst, b, pixelfmt_rgba8888) != result_OK)
    FAIL("bad bitmap");

  map = tonemap_create();
  if (map == NULL)
    FAIL("no tonemap");

  fill(a, PIXELFMT_MAKE_RGBA8888(10, 20, 30, 40));

  /* neutral map leaves the lower half alone */
  if (effects_tonemap_apply(&src, &dst, map) != result_OK ||
      b[0] != a[0])
  {
    tonemap_destroy(map);
    FAIL("neutral tonemap changed %08x to %08x", a[0], b[0]);
  }

  /* inverting red swaps the table ends */
  tonemap_get_values(map, tonemap_CHANNEL_RED, &spec);
  spec.flags = tonemap_FLAG_INVERT;
  tonemap_set(map, tonemap_CHANNEL_RED, &spec);

  tonemap_get_tables(map, &red, NULL, NULL, NULL);
  effects_tonemap_apply(&src, &dst, map);
  if (channel(b[0], 0) != red->level[10] || channel(b[0], 8) != 20)
  {
    tonemap_destroy(map);
    FAIL("inverted tonemap gave %08x", b[0]);
  }

  tonemap_destroy(map);

  return result_TEST_PASSED;
}

static result_t test_bad_args(void)
{
  bitmap_t     src, dst;
  unsigned int a[STRIDE * H], b[STRIDE * H];

  if (make(&src, a, pixelfmt_rgba8888) != result_OK ||
      make(&dst, b, pixelfmt_rgba8888) != result_OK)
    FAIL("bad bitmap");

  dst.format = pixelfmt_bgra8888;
  if (effects_grey_apply(&src, &dst) != result_BAD_ARG)
    FAIL("format mismatch accepted");

  dst.format = pixelfmt_rgba8888;
  dst.size.w--;
  if (effects_grey_apply(&src, &dst) != result_BAD_ARG)
    FAIL("size mismatch accepted");

  src.format = pixelfmt_p8;
  dst.size.w++;
  dst.format = pixelfmt_p8;
  if (effects_blur_apply(&src, &dst, effects_blur_BOX, 3) != result_BAD_ARG)
    FAIL("paletted bitmap accepted");

  return result_TEST_PASSED;
}

result_t effects_test(const char *resources)
{
  (void) resources;

  if (test_blur() != result_TEST_PASSED ||
      test_sharpen() != result_TEST_PASSED ||
      test_blend() != result_TEST_PASSED ||
      test_grey_clear() != result_TEST_PASSED ||
      test_levels() != result_TEST_PASSED ||
      test_tonemap() != result_TEST_PASSED ||
      test_bad_args() != result_TEST_PASSED)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}
