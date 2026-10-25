/* framebuf/dither/test/dither-test.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"

#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/dither.h"
#include "framebuf/pixelfmt.h"

#include "test/all-tests.h"

#define W 64
#define H 64

static unsigned int get_index(const bitmap_t *bm, int x, int y)
{
  const unsigned char *row;
  int                  log2bpp;

  row     = (const unsigned char *) bm->base + y * bm->rowbytes;
  log2bpp = pixelfmt_log2bpp(bm->format);

  switch (log2bpp)
  {
  case 0:  return (row[x >> 3] >> (7 - (x & 7))) & 1;
  case 1:  return (row[x >> 2] >> (6 - ((x & 3) << 1))) & 3;
  case 2:  return (row[x >> 1] >> ((x & 1) << 2)) & 15;
  default: return row[x];
  }
}

static void free_bitmap(bitmap_t *bm)
{
  if (bm == NULL)
    return;

  free(bm->base);
  free(bm->palette);
  free(bm);
}

static int same_pixels(const bitmap_t *a, const bitmap_t *b)
{
  int x, y;

  for (y = 0; y < H; y++)
    for (x = 0; x < W; x++)
      if (get_index(a, x, y) != get_index(b, x, y))
        return 0;

  return 1;
}

result_t dither_test(const char *resources)
{
  static const struct
  {
    int        ncolours;
    pixelfmt_t fmt;
  }
  formats[] =
  {
    { 1,   pixelfmt_p1 },
    { 2,   pixelfmt_p1 },
    { 3,   pixelfmt_p2 },
    { 16,  pixelfmt_p4 },
    { 17,  pixelfmt_p8 },
    { 256, pixelfmt_p8 }
  };

  result_t            rc;
  pixelfmt_rgbx8888_t pixels[W * H];
  bitmap_t            src;
  size2d_t            size = { W, H };
  colour_t            palette[256];
  dither_options_t    opts;
  bitmap_t           *out;
  bitmap_t           *ref;
  int                 method;
  unsigned long       sum;
  unsigned int        r, g, b;
  int                 mean;
  int                 x, y;
  int                 i;

  (void) resources;

  /* horizontal grey ramp */
  for (y = 0; y < H; y++)
    for (x = 0; x < W; x++)
      pixels[y * W + x] = PIXELFMT_MAKE_RGBA8888(x * 4, x * 4, x * 4, 255);

  rc = bitmap_init(&src, size, pixelfmt_rgbx8888, W * 4, NULL, pixels);
  if (rc != result_OK)
    return result_TEST_FAILED;

  for (i = 0; i < 256; i++)
    palette[i] = colour_rgb(i, 255 - i, (i * 7) & 255);

  memset(&opts, 0, sizeof(opts));
  opts.method     = dither_FLOYD_STEINBERG;
  opts.palette    = palette;
  opts.strength   = 100;
  opts.serpentine = 1;

  /* 1. output format follows the palette size; indices stay in range */
  for (i = 0; i < (int) (sizeof(formats) / sizeof(formats[0])); i++)
  {
    opts.ncolours = formats[i].ncolours;
    rc = dither_bitmap(&src, &opts, &out);
    if (rc != result_OK || out->format != formats[i].fmt)
    {
      printf("dither: %d colours gave format %d (rc %d)\n",
             opts.ncolours, out ? (int) out->format : -1, rc);
      free_bitmap(out);
      return result_TEST_FAILED;
    }
    for (y = 0; y < H; y++)
    {
      for (x = 0; x < W; x++)
      {
        if (get_index(out, x, y) >= (unsigned int) opts.ncolours)
        {
          printf("dither: index out of range for %d colours\n",
                 opts.ncolours);
          free_bitmap(out);
          return result_TEST_FAILED;
        }
      }
    }
    free_bitmap(out);
  }

  /* 2. each diffusing method preserves the ramp's mean against a two-colour
   * palette (Atkinson drops a quarter of the error, so it is excluded) */
  palette[0] = colour_rgb(0, 0, 0);
  palette[1] = colour_rgb(255, 255, 255);
  opts.ncolours = 2;

  for (method = dither_FLOYD_STEINBERG; method < dither__LIMIT; method++)
  {
    if (method == dither_ATKINSON)
      continue;

    opts.method = (dither_method_t) method;
    rc = dither_bitmap(&src, &opts, &out);
    if (rc != result_OK)
      return result_TEST_FAILED;

    sum = 0;
    for (y = 0; y < H; y++)
    {
      for (x = 0; x < W; x++)
      {
        colour_get_rgb(&out->palette[get_index(out, x, y)], &r, &g, &b);
        sum += r;
      }
    }
    free_bitmap(out);

    /* the ramp's mean is 126 */
    mean = (int) (sum / (W * H));
    if (mean < 122 || mean > 130)
    {
      printf("dither: method %d mean %d, expected ~126\n", method, mean);
      return result_TEST_FAILED;
    }
  }

  /* 3. serpentine differs from raster order */
  opts.method     = dither_FLOYD_STEINBERG;
  opts.serpentine = 1;
  rc = dither_bitmap(&src, &opts, &ref);
  if (rc != result_OK)
    return result_TEST_FAILED;

  opts.serpentine = 0;
  rc = dither_bitmap(&src, &opts, &out);
  if (rc != result_OK || same_pixels(ref, out))
  {
    printf("dither: serpentine made no difference\n");
    free_bitmap(ref);
    free_bitmap(out);
    return result_TEST_FAILED;
  }
  free_bitmap(ref);
  free_bitmap(out);

  /* 4. strength 0 matches no dithering */
  opts.method   = dither_NONE;
  opts.strength = 100;
  rc = dither_bitmap(&src, &opts, &ref);
  if (rc != result_OK)
    return result_TEST_FAILED;

  opts.method   = dither_STUCKI;
  opts.strength = 0;
  rc = dither_bitmap(&src, &opts, &out);
  if (rc != result_OK || !same_pixels(ref, out))
  {
    printf("dither: strength 0 differs from none\n");
    free_bitmap(ref);
    free_bitmap(out);
    return result_TEST_FAILED;
  }
  free_bitmap(ref);
  free_bitmap(out);

  /* 5. flat sRGB grey 128 against black and white: sRGB diffusion lights
   * about half the pixels, linear about 22% (128 decodes to 0.216) */
  for (i = 0; i < W * H; i++)
    pixels[i] = PIXELFMT_MAKE_RGBA8888(128, 128, 128, 255);

  opts.method     = dither_FLOYD_STEINBERG;
  opts.strength   = 100;
  opts.serpentine = 1;

  for (i = 0; i < 2; i++)
  {
    opts.linear = i;
    rc = dither_bitmap(&src, &opts, &out);
    if (rc != result_OK)
      return result_TEST_FAILED;

    sum = 0;
    for (y = 0; y < H; y++)
      for (x = 0; x < W; x++)
        sum += get_index(out, x, y);
    free_bitmap(out);

    mean = (int) (sum * 100 / (W * H));
    if (mean < (i ? 19 : 47) || mean > (i ? 25 : 53))
    {
      printf("dither: linear %d lit %d%% of pixels\n", i, mean);
      return result_TEST_FAILED;
    }
  }
  opts.linear = 0;

  /* 6. bad arguments */
  opts.strength = 101;
  if (dither_bitmap(&src, &opts, &out) != result_BAD_ARG)
  {
    printf("dither: bad strength accepted\n");
    return result_TEST_FAILED;
  }
  opts.strength = 100;
  opts.ncolours = 0;
  if (dither_bitmap(&src, &opts, &out) != result_BAD_ARG)
  {
    printf("dither: bad palette size accepted\n");
    return result_TEST_FAILED;
  }
  opts.ncolours = 2;
  src.format = pixelfmt_bgrx8888;
  if (dither_bitmap(&src, &opts, &out) != result_BAD_ARG)
  {
    printf("dither: bad format accepted\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}
