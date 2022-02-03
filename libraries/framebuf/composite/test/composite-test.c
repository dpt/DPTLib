/* composite-test.c */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "io/path.h"
#include "framebuf/bitmap.h"
#include "framebuf/screen.h"

#include "framebuf/composite.h"

/* ----------------------------------------------------------------------- */

#define SIZE        (256)
#define SMALLWIDTH  (SIZE)
#define SMALLHEIGHT (SIZE)
#define WIDTH       (4 * SMALLWIDTH)
#define HEIGHT      (3 * SMALLHEIGHT)
#define FORMAT      (pixelfmt_bgra8888)

/* ----------------------------------------------------------------------- */

/* Move these bitmap_* functions out to common, but the memory management
 * will need to be abstracted. */

static result_t bitmap_clone_by_size(bitmap_t *cloned, const bitmap_t *src)
{
  size_t pixelbytes;
  void  *pixels;

  assert(cloned);
  assert(src);

  pixelbytes = src->height * src->rowbytes;
  pixels = malloc(pixelbytes);
  if (pixels == NULL)
    return result_OOM;

  /* FIXME: This clones the palette pointer too, which is dubious. */

  *cloned = *src;
  cloned->base = pixels;

  return result_OK;
}

static result_t bitmap_clone_pixels(bitmap_t *dst, const bitmap_t *src)
{
  assert(dst);
  assert(src);

  if (dst->width    != src->width  ||
      dst->height   != src->height ||
      dst->format   != src->format ||
      dst->rowbytes != src->rowbytes)
    return result_INCOMPATIBLE;

  memcpy(dst->base, src->base, src->height * src->rowbytes);

  return result_OK;
}

static result_t bitmap_plot(const bitmap_t *src, bitmap_t *dst, int x, int y)
{
  pixelfmt_xxxa8888_t *sp = dst->base; /* FIXME: ought to have an xxxx_t type */
  pixelfmt_xxxa8888_t *dp = src->base;
  int                  h;

  if (src->format != dst->format)
    return result_INCOMPATIBLE;

  sp += x + y * dst->rowbytes / 4;
  for (h = 0; h < src->height; h++)
  {
    memcpy(sp, dp, src->rowbytes);
    sp += dst->rowbytes / 4;
    dp += src->rowbytes / 4;
  }

  return result_OK;
}

static result_t bitmap_convert_inplace(bitmap_t *bm, pixelfmt_t new_fmt)
{
  switch (bm->format)
  {
  case pixelfmt_rgbx8888:
    /* same format - assume x byte is 0xFF so ok for alpha */
    switch (new_fmt)
    {
    case pixelfmt_rgba8888:
      return result_OK;

    case pixelfmt_bgra8888:
      {
        pixelfmt_rgbx8888_t *p = bm->base;
        int                  x,y;

        for (y = 0; y < bm->height; y++)
        {
          for (x = 0; x < bm->width; x++)
          {
            pixelfmt_rgbx8888_t px = *p;
            *p++ = PIXELFMT_MAKE_BGRA8888(PIXELFMT_Bxxx8888(px),
                                          PIXELFMT_xGxx8888(px),
                                          PIXELFMT_xxRx8888(px),
                                          0xFF);
          }
        }

        bm->format = pixelfmt_bgra8888;
      }
      return result_OK;

    default:
      return result_NOT_IMPLEMENTED;
    }
    break;

  case pixelfmt_rgba8888:
    switch (new_fmt)
    {
    case pixelfmt_rgba8888:
      return result_OK; /* same format */

    case pixelfmt_bgra8888:
      {
        pixelfmt_rgba8888_t *p = bm->base;
        int                  x,y;

        for (y = 0; y < bm->height; y++)
        {
          for (x = 0; x < bm->width; x++)
          {
            pixelfmt_rgba8888_t px = *p;
            *p++ = PIXELFMT_MAKE_BGRA8888(PIXELFMT_Bxxx8888(px),
                                          PIXELFMT_xGxx8888(px),
                                          PIXELFMT_xxRx8888(px),
                                          PIXELFMT_xxxA8888(px));
          }
        }

        bm->format = pixelfmt_bgra8888;
      }
      return result_OK;

    default:
      return result_NOT_IMPLEMENTED;
    }

  default:
    return result_NOT_IMPLEMENTED;
  }
}

/* ----------------------------------------------------------------------- */

static result_t load_test_png(bitmap_t   *bm,
                              const char *resources,
                              const char *leafname)
{
  result_t    rc;
  const char *leafname_ext;
  const char *filename;

  leafname_ext = path_join_leafname(leafname, "png");
  filename     = path_join_filename(resources, 3, "resources", "composite", leafname_ext);

  rc = bitmap_load_png(bm, filename);
  if (rc)
    return rc;

  rc = bitmap_convert_inplace(bm, FORMAT);
  if (rc)
    return rc;

  if (bm->width != SMALLWIDTH || bm->height != SMALLHEIGHT || bm->format != FORMAT)
  {
    fprintf(stderr, "load_test_png: wrong width, height or format\n");
    free(bm->base);
    return result_BAD_ARG;
  }

  return result_OK;
}

/* ----------------------------------------------------------------------- */

/* This creates a large bitmap then draws all twelve composite rule
 * combinations into it, in a 4x3 grid. */
result_t composite_test(const char *resources)
{
  const int scr_rowbytes = (WIDTH << pixelfmt_log2bpp(FORMAT)) >> 3;

  result_t         rc;
  void            *bigpixels;
  bitmap_t         bigbitmap;
  bitmap_t         bm[2];
  bitmap_t         bmtmp;
  composite_rule_t rule;
  int              x,y;

  bigpixels = malloc(scr_rowbytes * HEIGHT);
  if (bigpixels == NULL)
  {
    rc = result_OOM;
    goto Failure;
  }

  bitmap_init(&bigbitmap, WIDTH, HEIGHT, FORMAT, scr_rowbytes, NULL, bigpixels);

  rc = load_test_png(&bm[0], resources, "A"); /* source */
  if (rc)
    goto Failure;

  rc = load_test_png(&bm[1], resources, "B"); /* destination */
  if (rc)
    goto Failure;

  rc = bitmap_clone_by_size(&bmtmp, &bm[1]);
  if (rc)
    goto Failure;

  rule = composite_RULE_CLEAR;
  for (y = 0; y < 3; y++)
  {
    for (x = 0; x < 4; x++)
    {
      if (rule >= composite_RULE__LIMIT)
        continue;

      /* reset the temporary output back to the 'destination' pixels */
      rc = bitmap_clone_pixels(&bmtmp, &bm[1]);
      if (rc)
        goto Failure;

      rc = composite(rule++, &bm[0], &bmtmp);
      if (rc)
        goto Failure;

      /* copy result to (x,y) inside bigscreen */
      bitmap_plot(&bmtmp, &bigbitmap, x * SMALLWIDTH, y * SMALLHEIGHT);
    }
  }

  rc = bitmap_save_png(&bigbitmap, path_join_leafname("composite-1", "png"));
  if (rc)
    goto Failure;

  free(bmtmp.base);
  free(bm[1].base);
  free(bm[0].base);
  free(bigpixels);

  return result_TEST_PASSED;


Failure:
  fprintf(stderr, "error: &%x\n", rc);
  return result_TEST_FAILED;
}

/* vim: set ts=8 sts=2 sw=2 et: */
