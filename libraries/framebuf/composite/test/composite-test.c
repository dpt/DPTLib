/* framebuf/composite/test/composite-test.c */

#include <math.h>
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

/* Checks composite_over_pixel's shortcuts and its general case. */
static int test_over_pixel(void)
{
  return composite_over_pixel(0xFF123456u, 0xFFFF0000u, 0) == 0xFF123456u &&
         composite_over_pixel(0xFF123456u, 0xFFFF0000u, 255) == 0xFFFF0000u &&
         composite_over_pixel(0x00000000u, 0xFF00FF00u, 128) == 0x8000FF00u &&
         composite_over_pixel(0xFF000000u, 0x80FFFFFFu, 255) == 0xFF808080u &&
         composite_over_pixel(0x800000FFu, 0x80FF0000u, 255) == 0xC0AA0055u;
}

/* Porter-Duff fractions: how much of each input survives a rule. */
enum { F_0, F_1, F_A, F_B, F_1_A, F_1_B };

static double fraction(int f, double a, double b)
{
  switch (f)
  {
  case F_1:
    return 1.0;
  case F_A:
    return a;
  case F_B:
    return b;
  case F_1_A:
    return 1.0 - a;
  case F_1_B:
    return 1.0 - b;
  default:
    return 0.0;
  }
}

/* Composites every pair of source and destination alphas under every rule
 * and checks each result is within 1 of the exact Porter-Duff value. A
 * channel past 255 would spill into its neighbour and fail too. */
static int test_rules_exact(void)
{
  static const unsigned char fractions[composite_RULE__LIMIT][2] =
  {
    { F_0,   F_0   }, /* CLEAR    */
    { F_1,   F_0   }, /* SRC      */
    { F_0,   F_1   }, /* DST      */
    { F_1,   F_1_A }, /* SRC_OVER */
    { F_1_B, F_1   }, /* DST_OVER */
    { F_B,   F_0   }, /* SRC_IN   */
    { F_0,   F_A   }, /* DST_IN   */
    { F_1_B, F_0   }, /* SRC_OUT  */
    { F_0,   F_1_A }, /* DST_OUT  */
    { F_B,   F_1_A }, /* SRC_ATOP */
    { F_1_B, F_A   }, /* DST_ATOP */
    { F_1_B, F_1_A }  /* XOR      */
  };
  static pixelfmt_xxxa8888_t srcpx[SIZE * SIZE];
  static pixelfmt_xxxa8888_t dstpx[SIZE * SIZE];

  bitmap_t                   src;
  bitmap_t                   dst;
  int                        rule;
  int                        x, y;
  pixelfmt_xxxa8888_t        s, d, o;
  double                     a, b, fa, fb, ao, want;
  int                        shift;
  int                        nfailures = 0;

  bitmap_init(&src, SIZE2D(SIZE, SIZE), FORMAT, SIZE * 4, NULL, srcpx);
  bitmap_init(&dst, SIZE2D(SIZE, SIZE), FORMAT, SIZE * 4, NULL, dstpx);

  for (rule = 0; rule < composite_RULE__LIMIT; rule++)
  {
    /* source alpha runs across, destination alpha down; the colours vary
     * so low alphas meet a spread of channel values */
    for (y = 0; y < SIZE; y++)
      for (x = 0; x < SIZE; x++)
      {
        srcpx[y * SIZE + x] = ((pixelfmt_xxxa8888_t) x << 24) |
                              (((x * 7 + y * 3) & 0xFFu) << 16) |
                              (0xFFu << 8) | 0xABu;
        dstpx[y * SIZE + x] = ((pixelfmt_xxxa8888_t) y << 24) |
                              (((x * 5 + y * 11) & 0xFFu) << 16) |
                              (0x00u << 8) | 0x55u;
      }

    if (composite((composite_rule_t) rule, &src, &dst))
      return 0;

    for (y = 0; y < SIZE; y++)
      for (x = 0; x < SIZE; x++)
      {
        s  = srcpx[y * SIZE + x];
        d  = ((pixelfmt_xxxa8888_t) y << 24) |
             (((x * 5 + y * 11) & 0xFFu) << 16) | 0x55u;
        o  = dstpx[y * SIZE + x];
        a  = x / 255.0;
        b  = y / 255.0;
        fa = fraction(fractions[rule][0], a, b);
        fb = fraction(fractions[rule][1], a, b);
        ao = a * fa + b * fb;

        if (fabs((double) (o >> 24) - ao * 255.0) > 1.0)
        {
          if (nfailures++ < 8)
            printf("rule %d sa=%d da=%d: alpha %u, want %.2f\n",
                   rule, x, y, (unsigned) (o >> 24), ao * 255.0);
          continue;
        }

        if ((o >> 24) == 0)
          continue; /* colour of a clear pixel is moot */

        for (shift = 0; shift < 24; shift += 8)
        {
          want = (((s >> shift) & 0xFF) * a * fa +
                  ((d >> shift) & 0xFF) * b * fb) / ao;
          if (fabs((double) ((o >> shift) & 0xFF) - want) > 1.0)
          {
            if (nfailures++ < 8)
              printf("rule %d sa=%d da=%d shift %d: %u, want %.2f\n",
                     rule, x, y, shift,
                     (unsigned) ((o >> shift) & 0xFF), want);
            break;
          }
        }
      }
  }

  printf("composite rules: %d failure(s)\n", nfailures);

  return nfailures == 0;
}

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

  if (!test_over_pixel() || !test_rules_exact())
  {
    rc = result_TEST_FAILED;
    goto Failure;
  }

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
