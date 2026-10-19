/* framebuf/bitmap/test/convert-test.c -- bitmap palette, clear and convert tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"

#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

#define FAIL(what) \
  do { printf("  failed: %s\n", what); goto Failure; } while (0)

/* source bitmaps carry this much padding at the end of every row, which the
 * converters must skip */
#define PAD 3

#define RGB_MASK 0x00FFFFFFu

/* A palette whose every entry differs, so a wrong index shows. */
static void mk_palette(colour_t *pal)
{
  int i;

  for (i = 0; i < 256; i++)
    pal[i] = colour_rgb(i, 255 - i, i ^ 0x5A);
}

static unsigned int expect_bgrx(int i)
{
  return ((unsigned int) i << 16) | ((unsigned int) (255 - i) << 8) |
         (unsigned int) (i ^ 0x5A);
}

static unsigned int expect_rgbx(int i)
{
  return (unsigned int) i | ((unsigned int) (255 - i) << 8) |
         ((unsigned int) (i ^ 0x5A) << 16);
}

/* Store index "v" at (x,y) of paletted "bm": p1 and p2 pack the leftmost
 * pixel in the high bits, p4 in the low nibble. */
static void put_index(bitmap_t *bm, int x, int y, int v)
{
  unsigned char *row;

  row = (unsigned char *) bm->base + y * bm->rowbytes;
  switch (bm->format)
  {
  case pixelfmt_p1:
    row[x >> 3] |= (unsigned char) (v << (7 - (x & 7)));
    break;
  case pixelfmt_p2:
    row[x >> 2] |= (unsigned char) (v << (6 - 2 * (x & 3)));
    break;
  case pixelfmt_p4:
    row[x >> 1] |= (unsigned char) (v << ((x & 1) * 4));
    break;
  default:
    row[x] = (unsigned char) v;
    break;
  }
}

/* The index stored at (x,y): varies along and down, wrapping at "n". */
static int index_at(int x, int y, int n)
{
  return (x * 3 + y * 5 + 1) % n;
}

/* Does "out", a 32bpp conversion of "src", hold the expected colours? */
static int check_converted(const bitmap_t *src,
                           const bitmap_t *out,
                           int             n,
                           int             rgbx)
{
  const unsigned int *row;
  unsigned int        want;
  int                 x, y;

  if (out->size.w != src->size.w || out->size.h != src->size.h)
    return 0;

  for (y = 0; y < src->size.h; y++)
  {
    row = (const unsigned int *) ((const char *) out->base + y * out->rowbytes);
    for (x = 0; x < src->size.w; x++)
    {
      want = rgbx ? expect_rgbx(index_at(x, y, n))
                  : expect_bgrx(index_at(x, y, n));
      if ((row[x] & RGB_MASK) != want)
        return 0;
    }
  }

  return 1;
}

static void free_converted(bitmap_t *bm)
{
  if (bm == NULL)
    return;

  free(bm->base);
  free(bm->palette);
  free(bm);
}

/* ----------------------------------------------------------------------- */

static int test_set_palette(void)
{
  colour_t  pal[256];
  bitmap_t  bm;
  colour_t *first;

  printf("test: set_palette copies, reuses, drops and ignores deep formats\n");

  mk_palette(pal);

  if (bitmap_init(&bm, SIZE2D(1, 1), pixelfmt_p4, 1, pal, NULL) != result_OK)
    return 0;
  first = bm.palette;
  if (first == NULL || first == pal || memcmp(first, pal, 16 * sizeof(*pal)))
    return 0;

  /* a second palette goes into the same buffer */
  if (bitmap_set_palette(&bm, pal + 16) != result_OK || bm.palette != first ||
      memcmp(bm.palette, pal + 16, 16 * sizeof(*pal)))
    return 0;

  if (bitmap_set_palette(&bm, NULL) != result_OK || bm.palette != NULL)
    return 0;

  /* deep formats have no palette to set */
  if (bitmap_init(&bm, SIZE2D(1, 1), pixelfmt_bgrx8888, 4, pal, NULL) !=
      result_OK || bm.palette != NULL)
    return 0;

  return 1;
}

/* ----------------------------------------------------------------------- */

static int test_clear(void)
{
  colour_t       pal[256];
  unsigned char  buf[64];
  unsigned short px16[2 * 4];
  unsigned int   px32[2 * 4];
  bitmap_t       bm;
  int            i;

  static const struct
  {
    pixelfmt_t    fmt;
    int           index;
    unsigned char byte;
  }
  packed[] =
  {
    { pixelfmt_p1, 1, 0xFF },
    { pixelfmt_p2, 2, 0xAA },
    { pixelfmt_p4, 5, 0x55 },
    { pixelfmt_p8, 7, 0x07 },
  };

  printf("test: clear fills every depth with the colour's pixel\n");

  mk_palette(pal);

  for (i = 0; i < (int) NELEMS(packed); i++)
  {
    if (bitmap_init(&bm, SIZE2D(8, 4), packed[i].fmt, 8, pal, buf) !=
        result_OK)
      return 0;
    memset(buf, 0, sizeof(buf));
    bitmap_clear(&bm, pal[packed[i].index]);
    bitmap_set_palette(&bm, NULL);
    if (buf[0] != packed[i].byte || buf[31] != packed[i].byte || buf[32] != 0)
    {
      printf("  failed: packed format %d\n", packed[i].fmt);
      return 0;
    }
  }

  /* 16bpp, with a padding pixel per row left alone */
  bitmap_init(&bm, SIZE2D(3, 2), pixelfmt_rgb565, 8, NULL, px16);
  memset(px16, 0, sizeof(px16));
  bitmap_clear(&bm, colour_rgb(255, 0, 0));
  if (px16[0] != 0x1F || px16[2] != 0x1F || px16[3] != 0 ||
      px16[4] != 0x1F || px16[6] != 0x1F || px16[7] != 0)
    FAIL("rgb565");

  /* 32bpp: opaque white is all 0xFF bytes, so takes a memset... */
  bitmap_init(&bm, SIZE2D(3, 2), pixelfmt_rgbx8888, 16, NULL, px32);
  memset(px32, 0, sizeof(px32));
  bitmap_clear(&bm, colour_rgb(255, 255, 255));
  if (px32[0] != 0xFFFFFFFFu || px32[6] != 0xFFFFFFFFu)
    FAIL("32bpp white");

  /* ...otherwise a pixel loop that leaves the padding alone */
  memset(px32, 0, sizeof(px32));
  bitmap_clear(&bm, colour_rgb(255, 0, 0));
  if ((px32[0] & RGB_MASK) != 0xFF || (px32[2] & RGB_MASK) != 0xFF ||
      px32[3] != 0 || (px32[4] & RGB_MASK) != 0xFF ||
      (px32[6] & RGB_MASK) != 0xFF || px32[7] != 0)
    FAIL("32bpp red");

  return 1;

Failure:
  return 0;
}

/* ----------------------------------------------------------------------- */

static int test_convert_paletted(void)
{
  static const struct
  {
    pixelfmt_t fmt;
    int        log2bpp;
  }
  fmts[] =
  {
    { pixelfmt_p1, 0 },
    { pixelfmt_p2, 1 },
    { pixelfmt_p4, 2 },
    { pixelfmt_p8, 3 },
  };

  colour_t      pal[256];
  unsigned char buf[16 * 8];
  unsigned int  into[13 * 3];
  bitmap_t      src;
  bitmap_t      dst;
  bitmap_t     *out;
  int           i;
  int           n;
  int           rowbytes;
  int           x, y;

  printf("test: convert and convert_into expand paletted bitmaps to 32bpp\n");

  mk_palette(pal);

  for (i = 0; i < (int) NELEMS(fmts); i++)
  {
    /* an odd width, so p4 has a lone last pixel and p1/p2 a part byte */
    n        = 1 << (1 << fmts[i].log2bpp);
    rowbytes = ((13 << fmts[i].log2bpp) + 7) / 8 + PAD;

    memset(buf, 0, sizeof(buf));
    if (bitmap_init(&src, SIZE2D(13, 3), fmts[i].fmt, rowbytes, pal, buf) !=
        result_OK)
      return 0;
    for (y = 0; y < 3; y++)
      for (x = 0; x < 13; x++)
        put_index(&src, x, y, index_at(x, y, n));

    if (bitmap_convert(&src, pixelfmt_bgrx8888, &out) != result_OK ||
        out->format != pixelfmt_bgrx8888 || !check_converted(&src, out, n, 0))
    {
      printf("  failed: format %d to bgrx8888\n", fmts[i].fmt);
      free_converted(out);
      goto Failure;
    }
    free_converted(out);

    if (bitmap_convert(&src, pixelfmt_rgbx8888, &out) != result_OK ||
        out->format != pixelfmt_rgbx8888 || !check_converted(&src, out, n, 1))
    {
      printf("  failed: format %d to rgbx8888\n", fmts[i].fmt);
      free_converted(out);
      goto Failure;
    }
    free_converted(out);

    if (bitmap_convert(&src, pixelfmt_rgb565, &out) != result_NOT_SUPPORTED ||
        out != NULL)
    {
      printf("  failed: format %d to rgb565 accepted\n", fmts[i].fmt);
      goto Failure;
    }

    bitmap_init(&dst, SIZE2D(13, 3), pixelfmt_bgrx8888, 13 * 4, NULL, into);
    if (bitmap_convert_into(&src, pixelfmt_bgrx8888, &dst) != result_OK ||
        !check_converted(&src, &dst, n, 0))
    {
      printf("  failed: format %d into bgrx8888\n", fmts[i].fmt);
      goto Failure;
    }

    /* only p8 converts into rgbx8888 */
    if (fmts[i].fmt == pixelfmt_p8)
    {
      bitmap_init(&dst, SIZE2D(13, 3), pixelfmt_rgbx8888, 13 * 4, NULL, into);
      if (bitmap_convert_into(&src, pixelfmt_rgbx8888, &dst) != result_OK ||
          !check_converted(&src, &dst, n, 1))
        FAIL("p8 into rgbx8888");
    }
    else if (bitmap_convert_into(&src, pixelfmt_rgbx8888, &dst) !=
             result_NOT_SUPPORTED)
    {
      printf("  failed: format %d into rgbx8888 accepted\n", fmts[i].fmt);
      goto Failure;
    }

    if (bitmap_convert_into(&src, pixelfmt_rgb565, &dst) !=
        result_NOT_SUPPORTED)
    {
      printf("  failed: format %d into rgb565 accepted\n", fmts[i].fmt);
      goto Failure;
    }

    bitmap_set_palette(&src, NULL);
  }

  return 1;

Failure:
  bitmap_set_palette(&src, NULL);
  return 0;
}

/* ----------------------------------------------------------------------- */

static int test_convert_16bpp(void)
{
  unsigned short px[2 * 3];
  unsigned int   out[2 * 2];
  bitmap_t       src;
  bitmap_t       dst;

  printf("test: convert_into widens 16bpp so full scale reaches 255\n");

  /* rgb565: red, green, blue, mid grey; one padding pixel per row */
  px[0] = (unsigned short) PIXELFMT_MAKE_RGB565(31, 0, 0);
  px[1] = (unsigned short) PIXELFMT_MAKE_RGB565(0, 63, 0);
  px[2] = 0xDEAD;
  px[3] = (unsigned short) PIXELFMT_MAKE_RGB565(0, 0, 31);
  px[4] = (unsigned short) PIXELFMT_MAKE_RGB565(16, 32, 16);
  px[5] = 0xDEAD;

  bitmap_init(&src, SIZE2D(2, 2), pixelfmt_rgb565, 6, NULL, px);
  bitmap_init(&dst, SIZE2D(2, 2), pixelfmt_bgrx8888, 8, NULL, out);
  if (bitmap_convert_into(&src, pixelfmt_bgrx8888, &dst) != result_OK)
    FAIL("rgb565");
  if ((out[0] & RGB_MASK) != 0xFF0000 || (out[1] & RGB_MASK) != 0x00FF00 ||
      (out[2] & RGB_MASK) != 0x0000FF || (out[3] & RGB_MASK) != 0x848284)
    FAIL("rgb565 pixels");

  /* rgbx5551: white, and five-bit 16 in every channel */
  px[0] = (unsigned short) PIXELFMT_MAKE_RGBX5551(31, 31, 31);
  px[1] = (unsigned short) PIXELFMT_MAKE_RGBX5551(16, 16, 16);
  bitmap_init(&src, SIZE2D(2, 1), pixelfmt_rgbx5551, 6, NULL, px);
  bitmap_init(&dst, SIZE2D(2, 1), pixelfmt_bgrx8888, 8, NULL, out);
  if (bitmap_convert_into(&src, pixelfmt_bgrx8888, &dst) != result_OK)
    FAIL("rgbx5551");
  if ((out[0] & RGB_MASK) != 0xFFFFFF || (out[1] & RGB_MASK) != 0x848484)
    FAIL("rgbx5551 pixels");

  if (bitmap_convert_into(&src, pixelfmt_rgbx8888, &dst) !=
      result_NOT_SUPPORTED)
    FAIL("rgbx5551 into rgbx8888 accepted");

  return 1;

Failure:
  return 0;
}

/* ----------------------------------------------------------------------- */

static int test_convert_unsupported(void)
{
  unsigned int px[1];
  bitmap_t     src;
  bitmap_t     dst;
  bitmap_t    *out;

  printf("test: compressed and deep sources are refused\n");

  bitmap_init(&dst, SIZE2D(1, 1), pixelfmt_bgrx8888, 4, NULL, px);

  bitmap_init(&src, SIZE2D(1, 1), pixelfmt_rgbx8888, 4, NULL, px);
  if (bitmap_convert(&src, pixelfmt_bgrx8888, &out) != result_NOT_SUPPORTED ||
      out != NULL)
    FAIL("convert deep");
  if (bitmap_convert_into(&src, pixelfmt_bgrx8888, &dst) !=
      result_NOT_SUPPORTED)
    FAIL("convert_into deep");

  /* only the format is looked at, so no real RLE data is needed */
  src.format = (pixelfmt_t) (pixelfmt_p8 | pixelfmt_RLE_FLAG);
  if (bitmap_convert(&src, pixelfmt_bgrx8888, &out) != result_NOT_SUPPORTED ||
      out != NULL)
    FAIL("convert compressed");
  if (bitmap_convert_into(&src, pixelfmt_bgrx8888, &dst) !=
      result_NOT_SUPPORTED)
    FAIL("convert_into compressed");

  return 1;

Failure:
  return 0;
}

/* ----------------------------------------------------------------------- */

result_t bitmap_convert_test(const char *resources)
{
  NOT_USED(resources);

  if (!test_set_palette())
  {
    printf("  failed: set_palette\n");
    return result_TEST_FAILED;
  }
  if (!test_clear() ||
      !test_convert_paletted() ||
      !test_convert_16bpp() ||
      !test_convert_unsupported())
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}
