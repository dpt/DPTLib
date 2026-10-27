/* framebuf/bitmap/test/load-test.c -- PNG loader tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"
#include "io/path.h"

#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

/* Fixtures are tiny PNGs in resources/test/bitmap/, one per colour type and
 * depth the loader accepts. */

static void load_free(bitmap_t *bm)
{
  free(bm->palette);
  free(bm->base);
}

/* Load "leaf" from the fixture directory, expecting a bitmap of the given
 * size and format. Returns 1 on success, leaving bm loaded. */
static int load_expect(const char *resources,
                       const char *leaf,
                       int         w,
                       int         h,
                       pixelfmt_t  fmt,
                       bitmap_t   *bm)
{
  result_t rc;

  rc = bitmap_load_png(bm, pathf("%s/resources/test/bitmap/%s",
                                 resources, leaf));
  if (rc != result_OK)
  {
    printf("load: %s: rc=&%x\n", leaf, rc);
    return 0;
  }

  if (bm->size.w != w || bm->size.h != h || bm->format != fmt)
  {
    printf("load: %s: got %dx%d format %d, want %dx%d format %d\n", leaf,
           bm->size.w, bm->size.h, (int) bm->format, w, h, (int) fmt);
    load_free(bm);
    return 0;
  }

  return 1;
}

static int row_is(const bitmap_t      *bm,
                  const char          *leaf,
                  int                  row,
                  const unsigned char *want,
                  int                  n)
{
  const unsigned char *got;
  int                  i;

  got = (const unsigned char *) bm->base + (size_t) row * bm->rowbytes;
  for (i = 0; i < n; i++)
  {
    if (got[i] != want[i])
    {
      printf("load: %s: row %d byte %d is 0x%02X, want 0x%02X\n",
             leaf, row, i, got[i], want[i]);
      return 0;
    }
  }

  return 1;
}

static int palette_is_grey(const bitmap_t *bm,
                           const char     *leaf,
                           int             index,
                           unsigned int    grey)
{
  unsigned int r, g, b;

  colour_get_rgb(&bm->palette[index], &r, &g, &b);
  if (r != grey || g != grey || b != grey ||
      colour_get_alpha(&bm->palette[index]) != 0xFF)
  {
    printf("load: %s: palette[%d] is %u,%u,%u, want grey %u\n",
           leaf, index, r, g, b, grey);
    return 0;
  }

  return 1;
}

/* A 1, 2, 4 or 8-bit greyscale PNG keeps its depth and gets a grey ramp for a
 * palette. 1 and 2-bit pixels pack leftmost in the high bits, as the p1 and
 * p2 screens do; 4-bit is swapped to the low-nibble-first p4 order. */
static int test_grey(const char *resources)
{
  static const unsigned char g1_0[] = { 0xB2 }, g1_1[] = { 0x4D };
  static const unsigned char g2_0[] = { 0x1B }, g2_1[] = { 0xE4 };
  static const unsigned char g4_0[] = { 0x50, 0xFA };
  static const unsigned char g4_1[] = { 0xAF, 0x05 };
  static const unsigned char g8_0[] = { 0, 64, 128, 255 };
  static const unsigned char g8_1[] = { 255, 128, 64, 0 };

  bitmap_t bm;
  int      ok;

  ok = 1;

  if (load_expect(resources, "grey1.png", 8, 2, pixelfmt_p1, &bm))
  {
    ok &= row_is(&bm, "grey1", 0, g1_0, 1);
    ok &= row_is(&bm, "grey1", 1, g1_1, 1);
    ok &= palette_is_grey(&bm, "grey1", 0, 0);
    ok &= palette_is_grey(&bm, "grey1", 1, 255);
    load_free(&bm);
  }
  else
  {
    ok = 0;
  }

  if (load_expect(resources, "grey2.png", 4, 2, pixelfmt_p2, &bm))
  {
    ok &= row_is(&bm, "grey2", 0, g2_0, 1);
    ok &= row_is(&bm, "grey2", 1, g2_1, 1);
    ok &= palette_is_grey(&bm, "grey2", 1, 85);
    ok &= palette_is_grey(&bm, "grey2", 2, 170);
    load_free(&bm);
  }
  else
  {
    ok = 0;
  }

  if (load_expect(resources, "grey4.png", 4, 2, pixelfmt_p4, &bm))
  {
    ok &= row_is(&bm, "grey4", 0, g4_0, 2);
    ok &= row_is(&bm, "grey4", 1, g4_1, 2);
    ok &= palette_is_grey(&bm, "grey4", 5, 85);
    ok &= palette_is_grey(&bm, "grey4", 15, 255);
    load_free(&bm);
  }
  else
  {
    ok = 0;
  }

  if (load_expect(resources, "grey8.png", 4, 2, pixelfmt_p8, &bm))
  {
    ok &= row_is(&bm, "grey8", 0, g8_0, 4);
    ok &= row_is(&bm, "grey8", 1, g8_1, 4);
    ok &= palette_is_grey(&bm, "grey8", 64, 64);
    load_free(&bm);
  }
  else
  {
    ok = 0;
  }

  return ok;
}

/* 16-bit greyscale is stripped to 8-bit. */
static int test_grey16(const char *resources)
{
  static const unsigned char want[] = { 0x00, 0xFF };

  bitmap_t bm;
  int      ok;

  if (!load_expect(resources, "grey16.png", 2, 1, pixelfmt_p8, &bm))
    return 0;

  ok = row_is(&bm, "grey16", 0, want, 2);
  load_free(&bm);

  return ok;
}

/* Grey+alpha is expanded to rgba8888: grey in all three channels, alpha
 * kept. */
static int test_grey_alpha(const char *resources)
{
  static const unsigned int grey[2][4]  = { { 0, 100, 200, 255 },
                                            { 255, 200, 100, 0 } };
  static const unsigned int alpha[2][4] = { { 255, 128, 0, 255 },
                                            { 255, 0, 128, 255 } };

  bitmap_t             bm;
  const unsigned char *row;
  int                  x, y;
  int                  ok;

  if (!load_expect(resources, "greyalpha8.png", 4, 2, pixelfmt_rgba8888, &bm))
    return 0;

  ok = 1;
  for (y = 0; y < 2; y++)
  {
    row = (const unsigned char *) bm.base + (size_t) y * bm.rowbytes;
    for (x = 0; x < 4; x++)
    {
      /* libpng byte order: R, G, B, A */
      if (row[x * 4 + 0] != grey[y][x] || row[x * 4 + 1] != grey[y][x] ||
          row[x * 4 + 2] != grey[y][x] || row[x * 4 + 3] != alpha[y][x])
      {
        printf("load: greyalpha8: (%d,%d) is %u,%u,%u,%u, want grey %u "
               "alpha %u\n", x, y, row[x * 4], row[x * 4 + 1],
               row[x * 4 + 2], row[x * 4 + 3], grey[y][x], alpha[y][x]);
        ok = 0;
      }
    }
  }

  load_free(&bm);

  return ok;
}

/* A paletted PNG stays paletted at its own depth, carrying PLTE across and
 * turning tRNS into palette alpha (entries past tRNS are opaque). */
static int test_palette(const char *resources)
{
  static const unsigned char want0[] = { 0x1B }, want1[] = { 0xE4 };
  static const unsigned int  rgb[4][3] =
  {
    { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 }, { 10, 20, 30 }
  };
  static const unsigned int  alpha[4] = { 0x40, 0x80, 0xFF, 0xFF };

  bitmap_t     bm;
  unsigned int r, g, b;
  int          i;
  int          ok;

  if (!load_expect(resources, "pal2.png", 4, 2, pixelfmt_p2, &bm))
    return 0;

  ok  = row_is(&bm, "pal2", 0, want0, 1);
  ok &= row_is(&bm, "pal2", 1, want1, 1);

  for (i = 0; i < 4; i++)
  {
    colour_get_rgb(&bm.palette[i], &r, &g, &b);
    if (r != rgb[i][0] || g != rgb[i][1] || b != rgb[i][2] ||
        colour_get_alpha(&bm.palette[i]) != alpha[i])
    {
      printf("load: pal2: palette[%d] is %u,%u,%u alpha %u\n", i, r, g, b,
             colour_get_alpha(&bm.palette[i]));
      ok = 0;
    }
  }

  load_free(&bm);

  return ok;
}

/* Paletted PNGs at the other depths: 1-bit keeps its bit order, 4-bit is
 * swapped to p4's low-nibble-first order, and 8-bit is read as is. tRNS
 * becomes palette alpha, and entries past the PLTE are opaque black. */
static int test_palette_depths(const char *resources)
{
  static const unsigned char p1_0[] = { 0xB2 }, p1_1[] = { 0x4D };
  static const unsigned char p4_0[] = { 0x50, 0xFA };
  static const unsigned char p4_1[] = { 0xAF, 0x05 };
  static const unsigned char p8_0[] = { 0, 1, 2, 3 };
  static const unsigned char p8_1[] = { 3, 2, 1, 0 };

  bitmap_t     bm;
  unsigned int r, g, b;
  int          ok;

  ok = 1;

  if (load_expect(resources, "pal1.png", 8, 2, pixelfmt_p1, &bm))
  {
    ok &= row_is(&bm, "pal1", 0, p1_0, 1);
    ok &= row_is(&bm, "pal1", 1, p1_1, 1);
    colour_get_rgb(&bm.palette[1], &r, &g, &b);
    if (r != 0 || g != 0 || b != 255 ||
        colour_get_alpha(&bm.palette[1]) != 255 ||
        colour_get_alpha(&bm.palette[0]) != 0x10)
    {
      printf("load: pal1: palette wrong\n");
      ok = 0;
    }
    load_free(&bm);
  }
  else
  {
    ok = 0;
  }

  if (load_expect(resources, "pal4.png", 4, 2, pixelfmt_p4, &bm))
  {
    ok &= row_is(&bm, "pal4", 0, p4_0, 2);
    ok &= row_is(&bm, "pal4", 1, p4_1, 2);
    colour_get_rgb(&bm.palette[5], &r, &g, &b);
    if (r != 80 || g != 175 || b != 40 ||
        colour_get_alpha(&bm.palette[5]) != 255 ||
        colour_get_alpha(&bm.palette[15]) != 0)
    {
      printf("load: pal4: palette wrong\n");
      ok = 0;
    }
    load_free(&bm);
  }
  else
  {
    ok = 0;
  }

  if (load_expect(resources, "pal8.png", 4, 2, pixelfmt_p8, &bm))
  {
    ok &= row_is(&bm, "pal8", 0, p8_0, 4);
    ok &= row_is(&bm, "pal8", 1, p8_1, 4);
    colour_get_rgb(&bm.palette[3], &r, &g, &b);
    if (r != 10 || g != 11 || b != 12)
    {
      printf("load: pal8: palette[3] wrong\n");
      ok = 0;
    }
    colour_get_rgb(&bm.palette[200], &r, &g, &b);
    if (r != 0 || g != 0 || b != 0 ||
        colour_get_alpha(&bm.palette[200]) != 255)
    {
      printf("load: pal8: unlisted entry is not opaque black\n");
      ok = 0;
    }
    load_free(&bm);
  }
  else
  {
    ok = 0;
  }

  return ok;
}

/* RGB gains an opaque filler byte; RGBA is read as is. */
static int test_deep(const char *resources)
{
  static const unsigned char rgb[]  = { 1, 2, 3, 0xFF, 250, 251, 252, 0xFF };
  static const unsigned char rgba[] = { 1, 2, 3, 4, 250, 251, 252, 253 };

  bitmap_t bm;
  int      ok;

  ok = 1;

  if (load_expect(resources, "rgb.png", 2, 1, pixelfmt_rgbx8888, &bm))
  {
    ok &= row_is(&bm, "rgb", 0, rgb, 8);
    load_free(&bm);
  }
  else
  {
    ok = 0;
  }

  if (load_expect(resources, "rgba.png", 2, 1, pixelfmt_rgba8888, &bm))
  {
    ok &= row_is(&bm, "rgba", 0, rgba, 8);
    load_free(&bm);
  }
  else
  {
    ok = 0;
  }

  return ok;
}

/* bitmap_load_png_deep expands a paletted PNG to the requested format and
 * leaves an RGB one as loaded. */
static int test_load_deep(const char *resources)
{
  static const unsigned char red[] = { 255, 0, 0 };
  static const unsigned char rgb[] = { 1, 2, 3 };

  result_t rc;
  bitmap_t bm;
  int      ok;

  ok = 1;

  rc = bitmap_load_png_deep(&bm, pathf("%s/resources/test/bitmap/pal2.png",
                                       resources), pixelfmt_rgbx8888);
  if (rc == result_OK)
  {
    if (bm.format != pixelfmt_rgbx8888 || bm.palette != NULL)
    {
      printf("load: deep pal2: format %d, palette %p\n", (int) bm.format,
             (void *) bm.palette);
      ok = 0;
    }
    ok &= row_is(&bm, "deep pal2", 0, red, 3);
    load_free(&bm);
  }
  else
  {
    printf("load: deep pal2: rc=&%x\n", rc);
    ok = 0;
  }

  rc = bitmap_load_png_deep(&bm, pathf("%s/resources/test/bitmap/rgb.png",
                                       resources), pixelfmt_rgbx8888);
  if (rc == result_OK)
  {
    if (bm.format != pixelfmt_rgbx8888)
    {
      printf("load: deep rgb: format %d\n", (int) bm.format);
      ok = 0;
    }
    ok &= row_is(&bm, "deep rgb", 0, rgb, 3);
    load_free(&bm);
  }
  else
  {
    printf("load: deep rgb: rc=&%x\n", rc);
    ok = 0;
  }

  return ok;
}

/* A colour-key tRNS on a truecolour or greyscale PNG makes the keyed colour
 * transparent. */
static int test_colour_key(const char *resources)
{
  static const unsigned char rgbkey[] = { 1, 2, 3, 0xFF, 250, 251, 252, 0 };

  bitmap_t bm;
  int      ok;

  ok = 1;

  if (load_expect(resources, "rgbkey.png", 2, 1, pixelfmt_rgba8888, &bm))
  {
    ok &= row_is(&bm, "rgbkey", 0, rgbkey, 8);
    load_free(&bm);
  }
  else
  {
    ok = 0;
  }

  /* the key is grey level 64: only that palette entry loses its alpha */
  if (load_expect(resources, "greykey.png", 4, 2, pixelfmt_p8, &bm))
  {
    if (colour_get_alpha(&bm.palette[64]) != 0 ||
        colour_get_alpha(&bm.palette[128]) != 0xFF)
    {
      printf("load: greykey: palette alpha wrong\n");
      ok = 0;
    }
    load_free(&bm);
  }
  else
  {
    ok = 0;
  }

  return ok;
}

/* Missing, non-PNG and unsupported-depth files are reported, not loaded. */
static int test_errors(const char *resources)
{
  static const struct
  {
    const char *leaf;
    result_t    want;
  }
  cases[] =
  {
    { "nonesuch.png", result_FILE_NOT_FOUND },
    { "notpng.png",   result_BAD_ARG },
    { "rgb16.png",    result_INCOMPATIBLE } /* only grey may be 16-bit */
  };

  bitmap_t bm;
  result_t rc;
  size_t   i;
  int      ok;

  ok = 1;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
  {
    rc = bitmap_load_png(&bm, pathf("%s/resources/test/bitmap/%s",
                                    resources, cases[i].leaf));
    if (rc != cases[i].want)
    {
      printf("load: %s: rc=&%x, want &%x\n", cases[i].leaf, rc,
             cases[i].want);
      ok = 0;
    }
  }

  return ok;
}

#ifndef DPTLIB_IMAGES_READ_ONLY

/* Saving each deep channel order writes R, G, B (and A) in PNG order, so each
 * reloads as the same rgbx8888 or rgba8888 bytes. A paletted bitmap with no
 * palette and an unwritable path are refused. */
static int test_save(void)
{
  static const struct
  {
    pixelfmt_t   fmt;
    unsigned int px; /* R=0x11, G=0x22, B=0x33, A=0x44 */
    int          alpha;
  }
  cases[] =
  {
    { pixelfmt_rgbx8888, 0xFF332211u, 0 },
    { pixelfmt_rgba8888, 0x44332211u, 1 },
    { pixelfmt_bgrx8888, 0xFF112233u, 0 },
    { pixelfmt_bgra8888, 0x44112233u, 1 },
    { pixelfmt_xbgr8888, 0x112233FFu, 0 },
    { pixelfmt_abgr8888, 0x11223344u, 1 },
    { pixelfmt_xrgb8888, 0x332211FFu, 0 },
    { pixelfmt_argb8888, 0x33221144u, 1 }
  };
  static const unsigned char rgb[]  = { 0x11, 0x22, 0x33, 0xFF };
  static const unsigned char rgba[] = { 0x11, 0x22, 0x33, 0x44 };

  const char   *fn;
  unsigned int  px;
  unsigned char p8;
  bitmap_t      bm;
  bitmap_t      rt;
  result_t      rc;
  size_t        i;
  int           ok;

  fn = pathf("%sload-test-save.png", DPTLIB_TMPPREFIX);
  ok = 1;
  for (i = 0; i < NELEMS(cases); i++)
  {
    px = cases[i].px;
    bitmap_init(&bm, SIZE2D(1, 1), cases[i].fmt, 4, NULL, &px);
    rc = bitmap_save_png(&bm, fn);
    if (rc == result_OK)
      rc = bitmap_load_png(&rt, fn);
    remove(fn);
    if (rc != result_OK)
    {
      printf("save: format %d: rc=&%x\n", (int) cases[i].fmt, rc);
      ok = 0;
      continue;
    }

    if (rt.format != (cases[i].alpha ? pixelfmt_rgba8888 : pixelfmt_rgbx8888))
    {
      printf("save: format %d: reloaded as format %d\n", (int) cases[i].fmt,
             (int) rt.format);
      ok = 0;
    }
    else
    {
      ok &= row_is(&rt, "save", 0, cases[i].alpha ? rgba : rgb, 4);
    }
    load_free(&rt);
  }

  p8 = 0;
  bitmap_init(&bm, SIZE2D(1, 1), pixelfmt_p8, 1, NULL, &p8);
  if (bitmap_save_png(&bm, fn) != result_NOT_SUPPORTED)
  {
    printf("save: p8 with no palette was not refused\n");
    ok = 0;
  }

  px = 0;
  bitmap_init(&bm, SIZE2D(1, 1), pixelfmt_rgbx8888, 4, NULL, &px);
  if (bitmap_save_png(&bm, "/nonexistent-dptlib/x.png") !=
      result_FOPEN_FAILED)
  {
    printf("save: unwritable path was not refused\n");
    ok = 0;
  }

  return ok;
}

#endif /* DPTLIB_IMAGES_READ_ONLY */

/* ----------------------------------------------------------------------- */

result_t bitmap_load_test(const char *resources)
{
  int ok;

  ok  = test_grey(resources);
  ok &= test_grey16(resources);
  ok &= test_grey_alpha(resources);
  ok &= test_palette(resources);
  ok &= test_palette_depths(resources);
  ok &= test_deep(resources);
  ok &= test_load_deep(resources);
  ok &= test_colour_key(resources);
  ok &= test_errors(resources);
#ifndef DPTLIB_IMAGES_READ_ONLY
  ok &= test_save();
#endif

  return ok ? result_TEST_PASSED : result_TEST_FAILED;
}
