/* framebuf/bitmap/test/convert-test.c -- bitmap palette, clear, convert and fill tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"

#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pattern.h"
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

static int test_convert_in_place(void)
{
  /* two pixels per row, then a padding word that must be left alone */
  static const unsigned int rgba[6] =
  {
    0x40302010, 0x80706050, 0xDEADBEEF,
    0x00CCBBAA, 0xFF332211, 0xDEADBEEF
  };
  static const unsigned int bgra[6] =
  {
    0x40102030, 0x80506070, 0xDEADBEEF,
    0x00AABBCC, 0xFF112233, 0xDEADBEEF
  };

  unsigned int px[6];
  bitmap_t     bm;
  int          i;

  printf("test: convert_in_place swaps between rgb/bgr x/a 8888\n");

  memcpy(px, rgba, sizeof(px));
  bitmap_init(&bm, SIZE2D(2, 2), pixelfmt_rgba8888, 3 * 4, NULL, px);
  if (bitmap_convert_in_place(&bm, pixelfmt_bgra8888) != result_OK ||
      bm.format != pixelfmt_bgra8888)
    FAIL("convert_in_place rgba");
  for (i = 0; i < 6; i++)
    if (px[i] != bgra[i])
      FAIL("convert_in_place rgba pixels");

  /* rgbx gains opaque alpha whatever its x byte held */
  memcpy(px, rgba, sizeof(px));
  bitmap_init(&bm, SIZE2D(2, 2), pixelfmt_rgbx8888, 3 * 4, NULL, px);
  if (bitmap_convert_in_place(&bm, pixelfmt_bgra8888) != result_OK)
    FAIL("convert_in_place rgbx");
  for (i = 0; i < 6; i++)
    if (px[i] != (i % 3 == 2 ? bgra[i] : (bgra[i] | 0xFF000000u)))
      FAIL("convert_in_place rgbx pixels");

  if (bitmap_convert_in_place(&bm, pixelfmt_bgra8888) != result_OK)
    FAIL("convert_in_place same format");
  if (bitmap_convert_in_place(&bm, pixelfmt_rgb565) != result_NOT_SUPPORTED)
    FAIL("convert_in_place unsupported");

  /* bgra back to rgbx swaps again and keeps the alpha byte */
  memcpy(px, bgra, sizeof(px));
  bitmap_init(&bm, SIZE2D(2, 2), pixelfmt_bgra8888, 3 * 4, NULL, px);
  if (bitmap_convert_in_place(&bm, pixelfmt_rgbx8888) != result_OK ||
      bm.format != pixelfmt_rgbx8888)
    FAIL("convert_in_place bgra to rgbx");
  for (i = 0; i < 6; i++)
    if (px[i] != rgba[i])
      FAIL("convert_in_place bgra to rgbx pixels");

  /* same order, x to alpha: only the top byte changes */
  memcpy(px, rgba, sizeof(px));
  bitmap_init(&bm, SIZE2D(2, 2), pixelfmt_rgbx8888, 3 * 4, NULL, px);
  if (bitmap_convert_in_place(&bm, pixelfmt_rgba8888) != result_OK)
    FAIL("convert_in_place rgbx to rgba");
  for (i = 0; i < 6; i++)
    if (px[i] != (i % 3 == 2 ? rgba[i] : (rgba[i] | 0xFF000000u)))
      FAIL("convert_in_place rgbx to rgba pixels");

  return 1;

Failure:
  return 0;
}

static int test_clone(void)
{
  colour_t     pal[256];
  unsigned int px[4] = { 1, 2, 3, 4 };
  bitmap_t     src;
  bitmap_t     dst;
  bitmap_t     other;

  printf("test: clone and copy_pixels\n");

  mk_palette(pal);
  bitmap_init(&src, SIZE2D(4, 1), pixelfmt_p8, 16, pal, px);

  if (bitmap_clone(&src, &dst) != result_OK)
    FAIL("clone");
  if (dst.base == src.base || dst.palette == src.palette ||
      dst.format != src.format || dst.rowbytes != src.rowbytes ||
      memcmp(dst.base, px, sizeof(px)) != 0 ||
      memcmp(dst.palette, pal, sizeof(pal)) != 0)
  {
    free(dst.base);
    free(dst.palette);
    FAIL("clone contents");
  }

  memset(dst.base, 0, sizeof(px));
  if (bitmap_copy_pixels(&src, &dst) != result_OK ||
      memcmp(dst.base, px, sizeof(px)) != 0)
  {
    free(dst.base);
    free(dst.palette);
    FAIL("copy_pixels");
  }

  other        = dst;
  other.size.w = 3;
  if (bitmap_copy_pixels(&src, &other) != result_INCOMPATIBLE)
  {
    free(dst.base);
    free(dst.palette);
    FAIL("copy_pixels mismatch");
  }

  free(dst.base);
  free(dst.palette);
  free(src.palette);

  return 1;

Failure:
  free(src.palette);
  return 0;
}

/* ----------------------------------------------------------------------- */

/* Reads pixel (x,y) of an 8, 16 or 32bpp bitmap. */
static unsigned int read_pixel(const bitmap_t *bm, int x, int y)
{
  const unsigned char *row;

  row = (const unsigned char *) bm->base + (size_t) y * bm->rowbytes;
  switch (pixelfmt_log2bpp(bm->format))
  {
  case 3:
    return row[x];
  case 4:
    return ((const unsigned short *) row)[x];
  default:
    return ((const unsigned int *) row)[x];
  }
}

/* Fills a cleared bitmap through a box with a phased pattern, plain then as
 * a stencil, and checks every pixel against the tile. */
static int fill_pattern_one(pixelfmt_t fmt, const colour_t *palette)
{
  static const box_t area = { 2, 1, 9, 4 };

  unsigned int px[10 * 5];
  bitmap_t     bm;
  pattern_t    pat;
  int          n;
  unsigned int fg;
  unsigned int bg;
  unsigned int old;
  int          pass;
  int          x, y;
  int          inside;
  int          set;
  unsigned int want;

  bitmap_init(&bm, SIZE2D(10, 5), fmt, 10 * 4, palette, px);
  memset(px, 0x11, sizeof(px));

  pat.bits[0] = 0xF0; pat.bits[1] = 0x81; pat.bits[2] = 0x3C;
  pat.bits[3] = 0xAA; pat.bits[4] = 0x55; pat.bits[5] = 0x00;
  pat.bits[6] = 0xFF; pat.bits[7] = 0x18;
  pat.fg     = palette ? palette[3] : colour_rgb(255, 0, 0);
  pat.bg     = palette ? palette[200] : colour_rgb(0, 0, 255);
  pat.origin = POINT(1, 2);

  n   = palette ? 256 : 0;
  fg  = (unsigned int) colour_to_pixel(palette, n, pat.fg, fmt);
  bg  = (unsigned int) colour_to_pixel(palette, n, pat.bg, fmt);
  old = read_pixel(&bm, 0, 0);

  if (palette && (fg != 3 || bg != 200))
    FAIL("palette indices");

  /* plain fill of the box, then a stencil over the whole bitmap in the
   * opposite colours: the clear bits outside the box keep the old value */
  for (pass = 0; pass < 2; pass++)
  {
    pat.flags = pass ? pattern_FLAG_STENCIL : 0;
    if (bitmap_fill_pattern(&bm, pass ? NULL : &area, &pat) != result_OK)
      FAIL("fill_pattern");

    for (y = 0; y < 5; y++)
      for (x = 0; x < 10; x++)
      {
        inside = x >= area.x0 && x < area.x1 && y >= area.y0 && y < area.y1;
        set    = (pat.bits[(y - pat.origin.y) & 7] >>
                  (7 - ((x - pat.origin.x) & 7))) & 1;
        if (set && (pass || inside))
          want = fg;
        else if (inside)
          want = bg;
        else
          want = old;
        if (read_pixel(&bm, x, y) != want)
        {
          printf("  fmt %d pass %d pixel (%d,%d) is %x, want %x\n",
                 fmt, pass, x, y, read_pixel(&bm, x, y), want);
          goto Failure;
        }
      }
  }

  /* a box wholly off the bitmap fills nothing */
  memset(px, 0x11, sizeof(px));
  pat.flags = 0;
  if (bitmap_fill_pattern(&bm, &(box_t) { 20, 20, 30, 30 }, &pat) !=
      result_OK || read_pixel(&bm, 9, 4) != old)
    FAIL("fill_pattern off the bitmap");

  return 1;

Failure:
  return 0;
}

static int test_fill_pattern(void)
{
  colour_t     pal[256];
  unsigned int px[1];
  bitmap_t     bm;
  pattern_t    pat;

  printf("test: fill_pattern at 8, 16 and 32bpp\n");

  mk_palette(pal);
  if (!fill_pattern_one(pixelfmt_p8, pal) ||
      !fill_pattern_one(pixelfmt_rgb565, NULL) ||
      !fill_pattern_one(pixelfmt_bgr565, NULL) ||
      !fill_pattern_one(pixelfmt_bgrx8888, NULL))
    return 0;

  memset(&pat, 0, sizeof(pat));
  bitmap_init(&bm, SIZE2D(1, 1), pixelfmt_p4, 4, pal, px);
  if (bitmap_fill_pattern(&bm, NULL, &pat) != result_NOT_SUPPORTED)
    FAIL("fill_pattern p4");
  bm.format = (pixelfmt_t) (pixelfmt_p8 | pixelfmt_RLE_FLAG);
  if (bitmap_fill_pattern(&bm, NULL, &pat) != result_NOT_SUPPORTED)
    FAIL("fill_pattern compressed");

  return 1;

Failure:
  return 0;
}

/* A ring of 1s with a gap at the top, around a hole of 0s, in a field of 0s:
 * a fill from inside leaks out through the gap, but not into the 2s.
 *
 *   0 0 0 0 0 0
 *   0 1 0 1 1 0
 *   0 1 0 0 1 0
 *   0 1 1 1 1 0
 *   2 2 2 2 2 2 */
static int test_flood_fill(void)
{
  static const unsigned int start[5][6] =
  {
    { 0, 0, 0, 0, 0, 0 },
    { 0, 1, 0, 1, 1, 0 },
    { 0, 1, 0, 0, 1, 0 },
    { 0, 1, 1, 1, 1, 0 },
    { 2, 2, 2, 2, 2, 2 }
  };
  unsigned int px[5][6];
  unsigned int fill;
  bitmap_t     bm;
  box_t        dirty;
  int          x, y;

  printf("test: flood_fill\n");

  memcpy(px, start, sizeof(px));
  bitmap_init(&bm, SIZE2D(6, 5), pixelfmt_bgrx8888, 6 * 4, NULL, px);
  fill = colour_to_pixel(NULL, 0, colour_rgb(255, 0, 0), pixelfmt_bgrx8888);

  if (bitmap_flood_fill(&bm, POINT(3, 2), colour_rgb(255, 0, 0), &dirty) !=
      result_OK)
    FAIL("flood_fill rc");
  for (y = 0; y < 5; y++)
    for (x = 0; x < 6; x++)
      if (px[y][x] != (start[y][x] == 0 ? fill : start[y][x]))
        FAIL("flood_fill pixel");
  if (dirty.x0 != 0 || dirty.y0 != 0 || dirty.x1 != 6 || dirty.y1 != 4)
    FAIL("flood_fill dirty");

  /* already the fill colour, and off the bitmap: nothing to do */
  if (bitmap_flood_fill(&bm, POINT(0, 0), colour_rgb(255, 0, 0), &dirty) !=
      result_OK || !box_is_empty(&dirty))
    FAIL("flood_fill same colour");
  if (bitmap_flood_fill(&bm, POINT(-1, 0), colour_rgb(0, 0, 0), &dirty) !=
      result_OK || !box_is_empty(&dirty))
    FAIL("flood_fill off bitmap");

  bm.format = pixelfmt_rgb565;
  if (bitmap_flood_fill(&bm, POINT(0, 0), colour_rgb(0, 0, 0), NULL) !=
      result_NOT_SUPPORTED)
    FAIL("flood_fill 16bpp");

  return 1;

Failure:
  return 0;
}

/* Restore box (1,0)-(5,4) around region (2,1)-(4,3) from a source with a
 * wider stride: only the ring between them takes the source's 0s. */
static int test_restore_outside_region(void)
{
  static const box_t keep = { 2, 1, 4, 3 };
  static const box_t area = { 1, 0, 5, 4 };
  unsigned int       dpx[4][6];
  unsigned int       spx[4][8];
  bitmap_t           dst, src;
  region_t          *region;
  int                x, y;
  int                inside_area, inside_keep;

  printf("test: restore_outside_region\n");

  for (y = 0; y < 4; y++)
    for (x = 0; x < 6; x++)
      dpx[y][x] = 1;
  memset(spx, 0, sizeof(spx));
  bitmap_init(&dst, SIZE2D(6, 4), pixelfmt_bgrx8888, 6 * 4, NULL, dpx);
  bitmap_init(&src, SIZE2D(6, 4), pixelfmt_bgrx8888, 8 * 4, NULL, spx);

  if (region_from_box(&keep, &region) != result_OK)
    FAIL("restore_outside_region region");

  if (bitmap_restore_outside_region(&dst, &src, region, &area) != result_OK)
    FAIL("restore_outside_region rc");
  for (y = 0; y < 4; y++)
    for (x = 0; x < 6; x++)
    {
      inside_area = box_contains_point(&area, x, y);
      inside_keep = box_contains_point(&keep, x, y);
      if (dpx[y][x] != (unsigned int) (inside_area && !inside_keep ? 0 : 1))
        FAIL("restore_outside_region pixel");
    }

  src.format = pixelfmt_rgb565;
  if (bitmap_restore_outside_region(&dst, &src, region, NULL) !=
      result_INCOMPATIBLE)
    FAIL("restore_outside_region format");

  region_destroy(region);
  return 1;

Failure:
  return 0;
}

static int test_has_transparency(void)
{
  unsigned int px[2][3];
  colour_t     pal[2];
  bitmap_t     bm;

  printf("test: has_transparency\n");

  /* opaque, then one translucent pixel at the end, in both alpha layouts */
  memset(px, 0xFF, sizeof(px));
  bitmap_init(&bm, SIZE2D(2, 2), pixelfmt_bgra8888, 3 * 4, NULL, px);
  px[0][2] = 0; /* in the row padding: must be ignored */
  if (bitmap_has_transparency(&bm))
    FAIL("has_transparency opaque");
  px[1][1] = 0x80FFFFFFu;
  if (!bitmap_has_transparency(&bm))
    FAIL("has_transparency bgra");
  bm.format = pixelfmt_abgr8888;
  if (bitmap_has_transparency(&bm))
    FAIL("has_transparency abgr opaque");
  px[1][1] = 0xFFFFFF80u;
  if (!bitmap_has_transparency(&bm))
    FAIL("has_transparency abgr");
  bm.format = pixelfmt_bgrx8888;
  if (bitmap_has_transparency(&bm))
    FAIL("has_transparency no alpha");

  /* paletted: judged by the palette */
  pal[0] = colour_rgb(0, 0, 0);
  pal[1] = colour_rgb(255, 255, 255);
  bitmap_init(&bm, SIZE2D(2, 2), pixelfmt_p1, 4, pal, px);
  if (bitmap_has_transparency(&bm))
    FAIL("has_transparency opaque palette");
  pal[1] = colour_rgba(255, 255, 255, 0);
  bitmap_set_palette(&bm, pal);
  if (!bitmap_has_transparency(&bm))
    FAIL("has_transparency palette");

  free(bm.palette);
  return 1;

Failure:
  free(bm.palette);
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
      !test_convert_unsupported() ||
      !test_convert_in_place() ||
      !test_clone() ||
      !test_fill_pattern() ||
      !test_flood_fill() ||
      !test_restore_outside_region() ||
      !test_has_transparency())
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}
