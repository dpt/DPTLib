/* framebuf/composite/test/composite-test.c */

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

static result_t bitmap_plot(const bitmap_t *src, bitmap_t *dst, int x, int y)
{
  pixelfmt_xxxa8888_t *sp = dst->base; /* FIXME: ought to have an xxxx_t type */
  pixelfmt_xxxa8888_t *dp = src->base;
  int                  h;

  if (src->format != dst->format)
    return result_INCOMPATIBLE;

  sp += x + y * dst->rowbytes / 4;
  for (h = 0; h < src->size.h; h++)
  {
    memcpy(sp, dp, src->rowbytes);
    sp += dst->rowbytes / 4;
    dp += src->rowbytes / 4;
  }

  return result_OK;
}

/* ----------------------------------------------------------------------- */

static result_t load_test_png(bitmap_t   *bm,
                              const char *resources,
                              const char *leafname)
{
  result_t    rc;
  const char *filename;

  filename = pathf("%s/resources/composite/%s.png", resources, leafname);

  rc = bitmap_load_png(bm, filename);
  if (rc)
    return rc;

  rc = bitmap_convert_in_place(bm, FORMAT);
  if (rc)
  {
    free(bm->base);
    return rc;
  }

  if (bm->size.w != SMALLWIDTH || bm->size.h != SMALLHEIGHT || bm->format != FORMAT)
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

  bitmap_init(&bigbitmap, SIZE2D(WIDTH, HEIGHT), FORMAT, scr_rowbytes, NULL, bigpixels);

  rc = load_test_png(&bm[0], resources, "A"); /* source */
  if (rc)
    goto Failure;

  rc = load_test_png(&bm[1], resources, "B"); /* destination */
  if (rc)
    goto Failure;

  rc = bitmap_clone(&bm[1], &bmtmp);
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
      rc = bitmap_copy_pixels(&bm[1], &bmtmp);
      if (rc)
        goto Failure;

      rc = composite(rule++, &bm[0], &bmtmp);
      if (rc)
        goto Failure;

      /* copy result to (x,y) inside bigscreen */
      bitmap_plot(&bmtmp, &bigbitmap, x * SMALLWIDTH, y * SMALLHEIGHT);
    }
  }

  rc = bitmap_save_png(&bigbitmap, pathf("%scomposite-1.png", DPTLIB_TMPPREFIX));
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
