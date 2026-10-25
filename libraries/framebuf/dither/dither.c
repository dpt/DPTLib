/* framebuf/dither/dither.c -- error-diffusion dithering to a palette */

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/pixelfmt.h"
#include "framebuf/pixelmap.h"

#include "framebuf/dither.h"

/* ----------------------------------------------------------------------- */

#define DITHER_MAX_W 5
#define DITHER_MAX_H 3
#define DITHER_PAD   2 /* columns either side absorbing off-image error */

/* coeff[0] is the current row; entries at or left of cx on it are zero */
typedef struct dither_kernel
{
  int           w, h;
  int           cx;      /* column of the current pixel */
  int           divisor;
  unsigned char coeff[DITHER_MAX_H][DITHER_MAX_W];
}
dither_kernel_t;

static const dither_kernel_t dither_kernels[dither__LIMIT] =
{
  /* none */
  { 1, 1, 0, 1, { { 0 } } },
  /* Floyd-Steinberg */
  { 3, 2, 1, 16, { { 0, 0, 7 },
                   { 3, 5, 1 } } },
  /* Jarvis, Judice and Ninke */
  { 5, 3, 2, 48, { { 0, 0, 0, 7, 5 },
                   { 3, 5, 7, 5, 3 },
                   { 1, 3, 5, 3, 1 } } },
  /* Stucki */
  { 5, 3, 2, 42, { { 0, 0, 0, 8, 4 },
                   { 2, 4, 8, 4, 2 },
                   { 1, 2, 4, 2, 1 } } },
  /* Atkinson */
  { 4, 3, 1, 8,  { { 0, 0, 1, 1 },
                   { 1, 1, 1, 0 },
                   { 0, 1, 0, 0 } } },
  /* Burkes */
  { 5, 2, 2, 32, { { 0, 0, 0, 8, 4 },
                   { 2, 4, 8, 4, 2 } } },
  /* Sierra */
  { 5, 3, 2, 32, { { 0, 0, 0, 5, 3 },
                   { 2, 4, 5, 4, 2 },
                   { 0, 2, 3, 2, 0 } } },
  /* Sierra two-row */
  { 5, 2, 2, 16, { { 0, 0, 0, 4, 3 },
                   { 1, 2, 3, 2, 1 } } },
  /* Sierra lite */
  { 3, 2, 1, 4,  { { 0, 0, 2 },
                   { 1, 1, 0 } } },
};

/* ----- sRGB <-> linear light ----- */

#define LINEAR_MAX  65535
#define ENCODE_BITS 12 /* linear-to-sRGB table has 2^ENCODE_BITS entries */

/* ponytail: same tables as bitmap/resize.c; share them if a third user
 * appears */
static struct
{
  unsigned short decode[256];
  unsigned char  encode[1 << ENCODE_BITS];
  int            ready;
}
dither_tables;

static void dither_make_tables(void)
{
  int    i;
  double c;

  if (dither_tables.ready)
    return;

  for (i = 0; i < 256; i++)
  {
    c = i / 255.0;
    c = c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
    dither_tables.decode[i] = (unsigned short) (c * LINEAR_MAX + 0.5);
  }

  for (i = 0; i < (1 << ENCODE_BITS); i++)
  {
    c = i / (double) ((1 << ENCODE_BITS) - 1);
    c = c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1 / 2.4) - 0.055;
    dither_tables.encode[i] = (unsigned char) (c * 255 + 0.5);
  }

  dither_tables.ready = 1;
}

/* ----------------------------------------------------------------------- */

/* add accumulated error `e`, scaled by `total`, to channel value `c`,
 * clamping to 0..max */
static unsigned int dither_apply(unsigned int c, int e, int total, int max)
{
  int v;

  v = (e >= 0) ? (e + total / 2) / total : -((-e + total / 2) / total);

  return (unsigned int) CLAMP((int) c + v, 0, max);
}

static unsigned int dither_lookup(const pixelmap_t *pm,
                                  unsigned int      r,
                                  unsigned int      g,
                                  unsigned int      b)
{
  unsigned int idx;
  unsigned int per_byte;
  unsigned int bit;

  idx = ((r >> (8 - pm->rbits)) << (pm->gbits + pm->bbits)) |
        ((g >> (8 - pm->gbits)) << pm->bbits) |
        ( b >> (8 - pm->bbits));

  if (pm->dest_log2bpp == 3)
    return pm->entries[idx];

  per_byte = 8u >> pm->dest_log2bpp;
  bit      = (idx % per_byte) << pm->dest_log2bpp;

  return (pm->entries[idx / per_byte] >> bit) &
         ((1u << (1u << pm->dest_log2bpp)) - 1);
}

/* Store a palette index using the bitmap module's packing: p4 low nibble
 * first, p1 and p2 most significant bits first. */
static void dither_put(unsigned char *row,
                       int            x,
                       int            log2bpp,
                       unsigned int   index)
{
  switch (log2bpp)
  {
  case 0:
    row[x >> 3] |= (unsigned char) (index << (7 - (x & 7)));
    break;
  case 1:
    row[x >> 2] |= (unsigned char) (index << (6 - ((x & 3) << 1)));
    break;
  case 2:
    row[x >> 1] |= (unsigned char) (index << ((x & 1) << 2));
    break;
  default:
    row[x] = (unsigned char) index;
    break;
  }
}

/* ----------------------------------------------------------------------- */

result_t dither_bitmap(const bitmap_t         *src,
                       const dither_options_t *opts,
                       bitmap_t              **dst)
{
  result_t                   rc;
  const dither_kernel_t     *k;
  int                        log2bpp;
  pixelfmt_t                 fmt;
  const pixelmap_t          *pm;
  colour_t                   palette[256];
  int                        rowbytes;
  unsigned char             *base;
  bitmap_t                  *out;
  int                        stride;
  int                       *err;
  int                        total;
  const unsigned short      *dec;
  int                        max;
  int                        i;
  int                        x, y;
  int                        cur;
  int                        dir;
  const unsigned char       *srcrow;
  unsigned char             *dstrow;
  const pixelfmt_rgbx8888_t *px;
  int                       *e;
  unsigned int               r, g, b;
  unsigned int               index;
  unsigned int               pr, pg, pb;
  int                        qr, qg, qb;
  int                        mx, my;
  int                        weight;

  *dst = NULL;

  if (src->format != pixelfmt_rgbx8888 && src->format != pixelfmt_rgba8888)
    return result_BAD_ARG;
  if (opts->method < 0 || opts->method >= dither__LIMIT ||
      opts->palette == NULL ||
      opts->ncolours < 1 || opts->ncolours > 256 ||
      opts->strength < 0 || opts->strength > 100)
    return result_BAD_ARG;

  k = &dither_kernels[opts->method];

  if (opts->ncolours <= 2)
  {
    log2bpp = 0;
    fmt     = pixelfmt_p1;
  }
  else if (opts->ncolours <= 4)
  {
    log2bpp = 1;
    fmt     = pixelfmt_p2;
  }
  else if (opts->ncolours <= 16)
  {
    log2bpp = 2;
    fmt     = pixelfmt_p4;
  }
  else
  {
    log2bpp = 3;
    fmt     = pixelfmt_p8;
  }

  pm = pixelmap_get(src->format, fmt, opts->palette, opts->ncolours);
  if (pm == NULL)
    return result_OOM;

  for (i = 0; i < 256; i++)
    palette[i] = (i < opts->ncolours) ? opts->palette[i] : colour_rgb(0, 0, 0);

  rowbytes = (((src->size.w << log2bpp) + 31) >> 5) << 2;

  base   = NULL;
  out    = NULL;
  err    = NULL;
  stride = src->size.w + 2 * DITHER_PAD;

  base = calloc((size_t) rowbytes * src->size.h, 1);
  out  = malloc(sizeof(*out));
  err  = calloc((size_t) stride * k->h * 3, sizeof(*err));
  if (base == NULL || out == NULL || err == NULL)
  {
    rc = result_OOM;
    goto failure;
  }

  rc = bitmap_init(out, src->size, fmt, rowbytes, palette, base);
  if (rc != result_OK)
    goto failure;

  /* error is stored scaled by divisor * 100 so that strength and the kernel
   * weights need only one division, at read time */
  total = k->divisor * 100;

  /* in linear light channels run 0..LINEAR_MAX through dec[], and are
   * re-encoded only for the nearest-colour lookup */
  dec = NULL;
  max = 255;
  if (opts->linear)
  {
    dither_make_tables();
    dec = dither_tables.decode;
    max = LINEAR_MAX;
  }

  srcrow = src->base;
  dstrow = base;
  for (y = 0; y < src->size.h; y++)
  {
    cur = y % k->h;
    dir = (opts->serpentine && (y & 1)) ? -1 : 1;
    px  = (const pixelfmt_rgbx8888_t *) srcrow;

    for (i = 0; i < src->size.w; i++)
    {
      x = (dir > 0) ? i : src->size.w - 1 - i;
      e = &err[(cur * stride + x + DITHER_PAD) * 3];

      r = PIXELFMT_Rxxx8888(px[x]);
      g = PIXELFMT_xGxx8888(px[x]);
      b = PIXELFMT_xxBx8888(px[x]);
      if (dec)
      {
        r = dec[r];
        g = dec[g];
        b = dec[b];
      }

      r = dither_apply(r, e[0], total, max);
      g = dither_apply(g, e[1], total, max);
      b = dither_apply(b, e[2], total, max);

      if (dec)
        index = dither_lookup(pm,
                              dither_tables.encode[r >> (16 - ENCODE_BITS)],
                              dither_tables.encode[g >> (16 - ENCODE_BITS)],
                              dither_tables.encode[b >> (16 - ENCODE_BITS)]);
      else
        index = dither_lookup(pm, r, g, b);
      dither_put(dstrow, x, log2bpp, index);

      if (opts->method == dither_NONE || opts->strength == 0)
        continue;

      colour_get_rgb(&palette[index], &pr, &pg, &pb);
      if (dec)
      {
        pr = dec[pr];
        pg = dec[pg];
        pb = dec[pb];
      }
      qr = ((int) r - (int) pr) * opts->strength;
      qg = ((int) g - (int) pg) * opts->strength;
      qb = ((int) b - (int) pb) * opts->strength;

      /* walking right to left mirrors the kernel horizontally */
      for (my = 0; my < k->h; my++)
      {
        for (mx = 0; mx < k->w; mx++)
        {
          weight = k->coeff[my][mx];
          if (weight == 0)
            continue;

          e = &err[(((cur + my) % k->h) * stride +
                    x + (mx - k->cx) * dir + DITHER_PAD) * 3];
          e[0] += qr * weight;
          e[1] += qg * weight;
          e[2] += qb * weight;
        }
      }
    }

    /* this row's slot becomes row y + k->h */
    memset(&err[cur * stride * 3], 0, (size_t) stride * 3 * sizeof(*err));

    srcrow += src->rowbytes;
    dstrow += rowbytes;
  }

  free(err);
  *dst = out;

  return result_OK;

failure:
  free(err);
  free(out);
  free(base);

  return rc;
}
