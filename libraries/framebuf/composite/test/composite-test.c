/* composite-test.c */

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

// This could move to common, but we would need to abstract the memory management.
static result_t bitmap_size_clone(bitmap_t *cloned, const bitmap_t *src)
{
  size_t pixelbytes;
  void  *pixels;

  pixelbytes = src->height * src->rowbytes;
  pixels = malloc(pixelbytes);
  if (pixels == NULL)
    return result_OOM;

  // TODO: This clones the palette pointer too, which is dubious.

  *cloned = *src;
  cloned->base = pixels;

  return result_OK;
}

static void bitmap_clone_pixels(bitmap_t *cloned, const bitmap_t *src)
{
  memcpy(cloned->base, src->base, src->height * src->rowbytes);
}

// todo: generalise and hoist out
static void bitmap_plot(const bitmap_t *src, bitmap_t *dst, int x, int y)
{
  pixelfmt_xxxa8888_t *sp = dst->base; // ought to have an xxxx_t type
  pixelfmt_xxxa8888_t *dp = src->base;
  int                  h;

  if (src->format != dst->format)
    return;

  sp = sp + x + y * dst->rowbytes / 4;
  for (h = 0; h < src->height; h++)
  {
    memcpy(sp, dp, src->rowbytes);
    sp += dst->rowbytes / 4;
    dp += src->rowbytes / 4;
  }
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
    fprintf(stderr, "error: wrong width, height or format\n");
    // TODO destroy bm
    return result_BAD_ARG;
  }

  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t composite_test(const char *resources)
{
  result_t         rc;
  void            *bigpixels;
  bitmap_t         bigbitmap;
  screen_t         bigscreen;
  bitmap_t         bmsrc;
  bitmap_t         bmdst;
  bitmap_t         bmtmp;
  composite_rule_t rule;
  int              x,y;

  const int scr_rowbytes = (WIDTH << pixelfmt_log2bpp(FORMAT)) / 8;

  bigpixels = malloc(scr_rowbytes * HEIGHT);
  if (bigpixels == NULL)
  {
    rc = result_OOM;
    goto Failure;
  }

  // make a big screen
  bitmap_init(&bigbitmap,
               WIDTH, HEIGHT,
               FORMAT,
               scr_rowbytes,
               NULL,
               bigpixels);

 // no point doing this: bitmap_clear(&bigbitmap, colour_rgb(0xFF, 0xFF, 0xFF));

  screen_for_bitmap(&bigscreen, &bigbitmap); // not actually using the screen yet

  rc = load_test_png(&bmsrc, resources, "A");
  if (rc)
    goto Failure;

  rc = load_test_png(&bmdst, resources, "B");
  if (rc)
    goto Failure;

  rc = bitmap_size_clone(&bmtmp, &bmdst);
  if (rc)
    goto Failure;

  // 12 composite rules, so want 4x3 grid of results?

  rule = composite_RULE_CLEAR;
  for (y = 0; y < 3; y++)
  {
    for (x = 0; x < 4; x++)
    {
      if (rule >= composite_RULE__LIMIT)
        continue;

      bitmap_clone_pixels(&bmtmp, &bmdst);

      rc = composite(rule++, &bmsrc, &bmtmp);
      if (rc)
        goto Failure;

      // copy bmdst to (x,y) inside bigscreen
      bitmap_plot(&bmtmp, &bigbitmap, x * SMALLWIDTH, y * SMALLHEIGHT);
    }
  }

  rc = bitmap_save_png(&bigbitmap, path_join_leafname("composite-1", "png"));
  if (rc)
    goto Failure;

  free(bigpixels);
  free(bmsrc.base);
  free(bmdst.base);
  free(bmtmp.base);

  return result_TEST_PASSED;


Failure:
  fprintf(stderr, "error: &%x\n", rc);
  return result_TEST_FAILED;
}

/* vim: set ts=8 sts=2 sw=2 et: */

