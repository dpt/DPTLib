/* framebuf/span/test/span-test.c -- span registry, copy, fill and blend tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"

#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/span.h"
#include "framebuf/span-registry.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

#define FAIL(what) \
  do { printf("  failed: %s\n", what); goto Failure; } while (0)

/* ----------------------------------------------------------------------- */

static int test_registry(void)
{
  static const pixelfmt_t fmts[] =
  {
    pixelfmt_p1, pixelfmt_p2, pixelfmt_p4, pixelfmt_p8,
    pixelfmt_rgbx5551, pixelfmt_rgb565,
    pixelfmt_bgrx8888, pixelfmt_rgbx8888, pixelfmt_xbgr8888,
  };

  const span_t *span;
  int           i;

  printf("test: registry finds a span for each format, NULL for others\n");

  for (i = 0; i < (int) NELEMS(fmts); i++)
  {
    /* twice: the second call comes from the cache */
    span = spanregistry_get(fmts[i]);
    if (span == NULL || span->format != fmts[i] || span->fill == NULL ||
        spanregistry_get(fmts[i]) != span)
    {
      printf("  failed: format %d\n", fmts[i]);
      return 0;
    }
  }

  if (spanregistry_get(pixelfmt_y8) != NULL)
    FAIL("y8 found");

  return 1;

Failure:
  return 0;
}

/* ----------------------------------------------------------------------- */

static int test_copy(void)
{
  static const pixelfmt_t fmts[] =
  {
    pixelfmt_p8, pixelfmt_rgb565, pixelfmt_bgrx8888,
  };

  unsigned char src[32];
  unsigned char dst[32];
  const span_t *span;
  int           bytes;
  int           i;

  printf("test: copy moves whole pixels and copying onto itself is a no-op\n");

  for (i = 0; i < (int) sizeof(src); i++)
    src[i] = (unsigned char) (i + 1);

  for (i = 0; i < (int) NELEMS(fmts); i++)
  {
    span  = spanregistry_get(fmts[i]);
    bytes = 3 << (pixelfmt_log2bpp(fmts[i]) - 3);

    memset(dst, 0, sizeof(dst));
    span->copy(dst, src, 3);
    if (memcmp(dst, src, bytes) != 0 || dst[bytes] != 0)
    {
      printf("  failed: format %d\n", fmts[i]);
      return 0;
    }

    span->copy(dst, dst, 3);
    if (memcmp(dst, src, bytes) != 0)
    {
      printf("  failed: format %d onto itself\n", fmts[i]);
      return 0;
    }
  }

  return 1;
}

/* ----------------------------------------------------------------------- */

static int test_fill(void)
{
  /* index 1 written to pixels 3..12 of a zeroed row, then of a row of all
   * ones bits: an odd start and end, so every format has part bytes at
   * both ends */
  static const struct
  {
    pixelfmt_t    fmt;
    unsigned char zeroed[8];
    unsigned char ones[8];
  }
  packed[] =
  {
    { pixelfmt_p1, { 0x1F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
                   { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } },
    { pixelfmt_p2, { 0x01, 0x55, 0x55, 0x40, 0x00, 0x00, 0x00, 0x00 },
                   { 0xFD, 0x55, 0x55, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF } },
    { pixelfmt_p4, { 0x00, 0x10, 0x11, 0x11, 0x11, 0x11, 0x01, 0x00 },
                   { 0xFF, 0x1F, 0x11, 0x11, 0x11, 0x11, 0xF1, 0xFF } },
  };

  unsigned char  row[8];
  unsigned short row16[6];
  unsigned int   row32[6];
  const span_t  *span;
  int            i;

  printf("test: fill writes only the run, at any start and length\n");

  for (i = 0; i < (int) NELEMS(packed); i++)
  {
    span = spanregistry_get(packed[i].fmt);

    memset(row, 0, sizeof(row));
    span->fill(row, 3, 1, 10);
    if (memcmp(row, packed[i].zeroed, sizeof(row)) != 0)
    {
      printf("  failed: format %d zeroed\n", packed[i].fmt);
      return 0;
    }

    /* the fill replaces, rather than ORs into, what was there */
    memset(row, 0xFF, sizeof(row));
    span->fill(row, 3, 1, 10);
    if (memcmp(row, packed[i].ones, sizeof(row)) != 0)
    {
      printf("  failed: format %d ones\n", packed[i].fmt);
      return 0;
    }
  }

  /* a zero length p4 run touches nothing */
  memset(row, 0xAB, sizeof(row));
  spanregistry_get(pixelfmt_p4)->fill(row, 3, 1, 0);
  if (row[1] != 0xAB)
    FAIL("p4 empty run");

  memset(row, 0, sizeof(row));
  spanregistry_get(pixelfmt_p8)->fill(row, 2, 0x1234, 3);
  if (row[1] != 0 || row[2] != 0x34 || row[4] != 0x34 || row[5] != 0)
    FAIL("p8");

  memset(row16, 0, sizeof(row16));
  spanregistry_get(pixelfmt_rgb565)->fill(row16, 2, 0xBEEF, 3);
  if (row16[1] != 0 || row16[2] != 0xBEEF || row16[4] != 0xBEEF ||
      row16[5] != 0)
    FAIL("rgb565");

  memset(row32, 0, sizeof(row32));
  spanregistry_get(pixelfmt_bgrx8888)->fill(row32, 2, 0xDEADBEEF, 3);
  if (row32[1] != 0 || row32[2] != 0xDEADBEEF || row32[4] != 0xDEADBEEF ||
      row32[5] != 0)
    FAIL("bgrx8888");

  return 1;

Failure:
  return 0;
}

/* ----------------------------------------------------------------------- */

static int test_blend_16bpp(void)
{
  static const unsigned char alphas[3] = { 0, 255, 128 };

  unsigned short src1[3];
  unsigned short src2[3];
  unsigned short dst[3];
  const span_t  *span;
  unsigned short white;
  unsigned short half;

  printf("test: 16bpp blends pass the ends through and mix in between\n");

  /* alpha 128 is adjusted to 129, so a full field f mixes to f * 129 >> 8 */
  span  = spanregistry_get(pixelfmt_rgb565);
  white = 0xFFFF;
  half  = (unsigned short) PIXELFMT_MAKE_RGB565(15, 31, 15);

  src1[0] = src1[1] = src1[2] = 0;
  src2[0] = src2[1] = src2[2] = white;

  span->blendconst(dst, src1, src2, 3, 0, NULL);
  if (dst[0] != 0 || dst[2] != 0)
    FAIL("rgb565 alpha 0");
  span->blendconst(dst, src1, src2, 3, 255, NULL);
  if (dst[0] != white || dst[2] != white)
    FAIL("rgb565 alpha 255");
  span->blendconst(dst, src1, src2, 3, 128, NULL);
  if (dst[0] != half || dst[2] != half)
    FAIL("rgb565 alpha 128");

  span->blendarray(dst, src1, src2, 3, alphas);
  if (dst[0] != 0 || dst[1] != white || dst[2] != half)
    FAIL("rgb565 alpha array");

  span  = spanregistry_get(pixelfmt_rgbx5551);
  white = (unsigned short) PIXELFMT_MAKE_RGBX5551(31, 31, 31);
  half  = (unsigned short) PIXELFMT_MAKE_RGBX5551(15, 15, 15);

  src2[0] = src2[1] = src2[2] = white;

  span->blendconst(dst, src1, src2, 3, 128, NULL);
  if (dst[0] != half || dst[2] != half)
    FAIL("rgbx5551 alpha 128");

  span->blendarray(dst, src1, src2, 3, alphas);
  if (dst[0] != 0 || dst[1] != white || dst[2] != half)
    FAIL("rgbx5551 alpha array");

  return 1;

Failure:
  return 0;
}

/* ----------------------------------------------------------------------- */

static int test_blend_paletted(void)
{
  static const struct
  {
    pixelfmt_t fmt;
    int        log2bpp;
    int        slack;  /* nearest-colour tables may be off by this much */
  }
  fmts[] =
  {
    { pixelfmt_p1, 0, 0 },
    { pixelfmt_p2, 1, 0 },
    { pixelfmt_p4, 2, 17 },
    { pixelfmt_p8, 3, 8 },
  };
  static const int alphas[] = { 0, 255, 170 };

  colour_t      pal[256];
  colour_t      src2[3];
  unsigned char src1[3];
  unsigned char dst[3];
  const span_t *span;
  int           i;
  int           n;
  int           j;
  int           k;
  int           want;
  int           got;

  printf("test: paletted blends mix in RGB then pick the nearest entry\n");

  for (i = 0; i < (int) NELEMS(fmts); i++)
  {
    /* an even grey ramp from black to white */
    n = 1 << (1 << fmts[i].log2bpp);
    for (j = 0; j < n; j++)
      pal[j] = colour_rgb(j * 255 / (n - 1), j * 255 / (n - 1),
                          j * 255 / (n - 1));

    span = spanregistry_get(fmts[i].fmt);

    /* black under white */
    for (j = 0; j < 3; j++)
    {
      src1[j] = 0;
      src2[j] = pal[n - 1];
    }

    /* the ends pass through and two thirds of the way lands on a p2
     * entry and near p4 and p8 ones; p1 rounds it up to white. The
     * nearest-colour tables are coarse, so compare colours not indices. */
    for (k = 0; k < (int) NELEMS(alphas); k++)
    {
      span->blendconst(dst, src1, src2, 3, alphas[k], pal);
      want = (n == 2 && alphas[k] == 170) ? 255 : alphas[k];
      got  = PIXELFMT_Rxxx8888(pal[dst[2]].primary);
      if (dst[0] != dst[2] || abs(got - want) > fmts[i].slack)
      {
        printf("  failed: format %d alpha %d gave %d\n", fmts[i].fmt,
               alphas[k], got);
        return 0;
      }
    }
  }

  return 1;
}

/* ----------------------------------------------------------------------- */

result_t span_test(const char *resources)
{
  NOT_USED(resources);

  if (!test_registry() ||
      !test_copy() ||
      !test_fill() ||
      !test_blend_16bpp() ||
      !test_blend_paletted())
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}
