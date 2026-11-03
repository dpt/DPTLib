/* resize.c -- resample a 32bpp bitmap to a new size */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "framebuf/colourspace.h"

/* ----------------------------------------------------------------------- */

#define FRAC_BITS 16

/* Source coordinate, in 16.16, of the centre of destination pixel i. */
static int64_t resize_centre(int i, int srclen, int dstlen)
{
  return (((int64_t) (2 * i + 1) * srclen << FRAC_BITS) / (2 * dstlen)) -
         (1 << (FRAC_BITS - 1));
}

/* Interpolate four sample values; fx, fy are 0..65536. */
static int64_t resize_lerp(int64_t v00,
                           int64_t v10,
                           int64_t v01,
                           int64_t v11,
                           int     fx,
                           int     fy)
{
  int64_t top, bottom;

  top    = (v00 << FRAC_BITS) + (v10 - v00) * fx;
  bottom = (v01 << FRAC_BITS) + (v11 - v01) * fx;

  return (((top << FRAC_BITS) + (bottom - top) * fy) +
          ((int64_t) 1 << (2 * FRAC_BITS - 1))) >> (2 * FRAC_BITS);
}

/* ----- sRGB <-> linear light ----- */

#define LINEAR_MAX   colourspace_LINEAR_MAX
#define ENCODE_SIZE  (1 << colourspace_ENCODE_BITS)

static struct
{
  const uint16_t *decode;
  const uint8_t  *encode;
}
g;

static void resize_make_tables(void)
{
  g.decode = colourspace_srgb_decode();
  g.encode = colourspace_srgb_encode();
}

/* Lane of a 32bpp pixel that is not a colour (alpha or padding): always
 * interpolated as stored. */
static int resize_other_lane(pixelfmt_t fmt)
{
  switch (fmt)
  {
  case pixelfmt_argb8888:
  case pixelfmt_abgr8888:
  case pixelfmt_xrgb8888:
  case pixelfmt_xbgr8888:
    return 0;

  default:
    return 3;
  }
}

/* A destination column's source span for the box filter: from f0 to f1,
 * covering whole source columns s0 to s1 - 1. */
typedef struct resize_box_col
{
  double f0, f1;
  int    s0, s1;
}
resize_box_col_t;

/* Box filter: each destination pixel is the area-weighted average of the
 * source rectangle it covers. The column spans are worked out once, not on
 * every row. */
static result_t resize_box(const bitmap_t *src,
                           uint8_t        *base,
                           size2d_t        size,
                           int             linear,
                           int             other)
{
  resize_box_col_t *cols;
  int               x, y, c;
  int               sx, sy;
  int               sx0, sx1, sy0, sy1;
  double            fx0, fx1, fy0, fy1;
  double            wx, wy, w, total;
  double            acc[4];
  const uint8_t    *sp;
  uint8_t          *px;
  int               v;

  cols = malloc((size_t) size.w * sizeof(*cols));
  if (cols == NULL)
    return result_OOM;

  for (x = 0; x < size.w; x++)
  {
    cols[x].f0 = (double) x * src->size.w / size.w;
    cols[x].f1 = (double) (x + 1) * src->size.w / size.w;
    cols[x].s0 = (int) cols[x].f0;
    cols[x].s1 = MIN((int) ceil(cols[x].f1), src->size.w);
  }

  for (y = 0; y < size.h; y++)
  {
    fy0 = (double) y * src->size.h / size.h;
    fy1 = (double) (y + 1) * src->size.h / size.h;
    sy0 = (int) fy0;
    sy1 = MIN((int) ceil(fy1), src->size.h);
    px  = base + (size_t) y * size.w * 4;

    for (x = 0; x < size.w; x++, px += 4)
    {
      fx0 = cols[x].f0;
      fx1 = cols[x].f1;
      sx0 = cols[x].s0;
      sx1 = cols[x].s1;

      acc[0] = acc[1] = acc[2] = acc[3] = 0;
      total  = 0;
      for (sy = sy0; sy < sy1; sy++)
      {
        wy = MIN(fy1, sy + 1.0) - MAX(fy0, (double) sy);
        sp = (const uint8_t *) src->base + (size_t) sy * src->rowbytes +
             (size_t) sx0 * 4;
        for (sx = sx0; sx < sx1; sx++, sp += 4)
        {
          wx = MIN(fx1, sx + 1.0) - MAX(fx0, (double) sx);
          w  = wx * wy;
          total += w;
          for (c = 0; c < 4; c++)
            acc[c] += w * (linear && c != other ? g.decode[sp[c]]
                                                : sp[c]);
        }
      }

      for (c = 0; c < 4; c++)
      {
        acc[c] /= total;
        if (linear && c != other)
        {
          v = (int) (acc[c] * (ENCODE_SIZE - 1) / LINEAR_MAX + 0.5);
          px[c] = g.encode[MIN(v, ENCODE_SIZE - 1)];
        }
        else
        {
          px[c] = (uint8_t) (acc[c] + 0.5);
        }
      }
    }
  }

  free(cols);

  return result_OK;
}

/* Catmull-Rom weights for the four taps around a sample at fraction t. */
static void resize_cubic_weights(double t, double w[4])
{
  double t2, t3;

  t2   = t * t;
  t3   = t2 * t;
  w[0] = (-t3 + 2 * t2 - t) / 2;
  w[1] = (3 * t3 - 5 * t2 + 2) / 2;
  w[2] = (-3 * t3 + 4 * t2 + t) / 2;
  w[3] = (t3 - t2) / 2;
}

/* A destination column's taps for the cubic filter: the source column at
 * or left of its centre and the four weights round it. */
typedef struct resize_cubic_col
{
  int    ix;
  double w[4];
}
resize_cubic_col_t;

/* Bicubic (Catmull-Rom) over a 4x4 neighbourhood, clamped at the edges.
 * Sharper than bilinear but can overshoot, so the result is clamped. The
 * column taps are worked out once, not on every row. */
static result_t resize_cubic(const bitmap_t *src,
                             uint8_t        *base,
                             size2d_t        size,
                             int             linear,
                             int             other)
{
  resize_cubic_col_t *cols;
  int                 x, y, c, i, j;
  int                 ix, iy, sx, sy;
  double              fx, fy;
  const double       *wx;
  double              wy[4];
  double              acc[4], row[4];
  const uint8_t      *sp;
  uint8_t            *px;
  int                 v, max;

  cols = malloc((size_t) size.w * sizeof(*cols));
  if (cols == NULL)
    return result_OOM;

  for (x = 0; x < size.w; x++)
  {
    fx         = (x + 0.5) * src->size.w / size.w - 0.5;
    cols[x].ix = (int) floor(fx);
    resize_cubic_weights(fx - cols[x].ix, cols[x].w);
  }

  for (y = 0; y < size.h; y++)
  {
    fy = (y + 0.5) * src->size.h / size.h - 0.5;
    iy = (int) floor(fy);
    resize_cubic_weights(fy - iy, wy);
    px = base + (size_t) y * size.w * 4;

    for (x = 0; x < size.w; x++, px += 4)
    {
      ix = cols[x].ix;
      wx = cols[x].w;

      acc[0] = acc[1] = acc[2] = acc[3] = 0;
      for (j = 0; j < 4; j++)
      {
        sy = CLAMP(iy - 1 + j, 0, src->size.h - 1);
        row[0] = row[1] = row[2] = row[3] = 0;
        for (i = 0; i < 4; i++)
        {
          sx = CLAMP(ix - 1 + i, 0, src->size.w - 1);
          sp = (const uint8_t *) src->base + (size_t) sy * src->rowbytes +
               (size_t) sx * 4;
          for (c = 0; c < 4; c++)
            row[c] += wx[i] * (linear && c != other ? g.decode[sp[c]]
                                                    : sp[c]);
        }
        for (c = 0; c < 4; c++)
          acc[c] += wy[j] * row[c];
      }

      for (c = 0; c < 4; c++)
      {
        if (linear && c != other)
        {
          v = (int) (acc[c] * (ENCODE_SIZE - 1) / LINEAR_MAX + 0.5);
          px[c] = g.encode[CLAMP(v, 0, ENCODE_SIZE - 1)];
        }
        else
        {
          max   = 255;
          v     = (int) floor(acc[c] + 0.5);
          px[c] = (uint8_t) CLAMP(v, 0, max);
        }
      }
    }
  }

  free(cols);

  return result_OK;
}

/* Source byte offsets of the two pixels either side of a destination column
 * and the 16.16 weight of the second. Nearest uses x0 alone. */
typedef struct resize_col
{
  int x0, x1;
  int wx;
}
resize_col_t;

/* Fill one entry per destination column, so the row loops do no division. */
static void resize_make_cols(resize_col_t     *cols,
                             int               srclen,
                             int               dstlen,
                             bitmap_resample_t method)
{
  int     x;
  int64_t fx;
  int     x0;

  for (x = 0; x < dstlen; x++)
  {
    if (method == bitmap_resample_NEAREST)
    {
      cols[x].x0 = (int) (((int64_t) x * srclen) / dstlen) * 4;
      continue;
    }

    fx = resize_centre(x, srclen, dstlen);
    x0 = (int) (fx >> FRAC_BITS); /* arithmetic shift floors */
    cols[x].wx = (int) (fx - ((int64_t) x0 * (1 << FRAC_BITS)));
    cols[x].x0 = CLAMP(x0, 0, srclen - 1) * 4;
    cols[x].x1 = CLAMP(x0 + 1, 0, srclen - 1) * 4;
  }
}

result_t bitmap_resize(const bitmap_t   *src,
                       size2d_t          size,
                       bitmap_resample_t method,
                       int               linear,
                       bitmap_t        **out)
{
  result_t       rc;
  bitmap_t      *dst;
  uint8_t       *base;
  resize_col_t  *cols;
  int            dstrow;
  int            x, y, c;
  int64_t        fy;
  int            x0, x1, y0, y1;
  int            wx, wy;
  const uint8_t *row0, *row1;
  uint8_t       *px;
  int            other;
  int64_t        v;

  *out = NULL;

  if (size.w <= 0 || size.h <= 0 || src->size.w <= 0 || src->size.h <= 0)
    return result_BAD_ARG;

  if (pixelfmt_is_rle(src->format) || pixelfmt_log2bpp(src->format) != 5)
    return result_NOT_SUPPORTED;

  other = resize_other_lane(src->format);
  if (linear)
    resize_make_tables();

  dstrow = size.w * 4;
  dst    = malloc(sizeof(*dst));
  base   = malloc((size_t) dstrow * size.h);
  if (dst == NULL || base == NULL)
  {
    free(dst);
    free(base);
    return result_OOM;
  }

  *dst         = *src;
  dst->size    = size;
  dst->rowbytes = dstrow;
  dst->palette = NULL;
  dst->base    = base;

  /* box only helps when shrinking; otherwise it is bilinear */
  if (method == bitmap_resample_BOX &&
      size.w >= src->size.w && size.h >= src->size.h)
    method = bitmap_resample_BILINEAR;

  if (method == bitmap_resample_CUBIC || method == bitmap_resample_BOX)
  {
    rc = (method == bitmap_resample_CUBIC)
         ? resize_cubic(src, base, size, linear, other)
         : resize_box(src, base, size, linear, other);
    if (rc != result_OK)
    {
      free(dst);
      free(base);
      return rc;
    }
    *out = dst;
    return result_OK;
  }

  cols = malloc((size_t) size.w * sizeof(*cols));
  if (cols == NULL)
  {
    free(dst);
    free(base);
    return result_OOM;
  }
  resize_make_cols(cols, src->size.w, size.w, method);

  for (y = 0; y < size.h; y++)
  {
    px = base + (size_t) y * dstrow;

    if (method == bitmap_resample_NEAREST)
    {
      y0   = (int) (((int64_t) y * src->size.h) / size.h);
      row0 = (const uint8_t *) src->base + (size_t) y0 * src->rowbytes;
      for (x = 0; x < size.w; x++, px += 4)
      {
        x0 = cols[x].x0;
        for (c = 0; c < 4; c++)
          px[c] = row0[x0 + c];
      }
      continue;
    }

    fy = resize_centre(y, src->size.h, size.h);
    y0 = (int) (fy >> FRAC_BITS); /* arithmetic shift floors */
    wy = (int) (fy - ((int64_t) y0 * (1 << FRAC_BITS)));
    y1 = y0 + 1;
    y0 = CLAMP(y0, 0, src->size.h - 1);
    y1 = CLAMP(y1, 0, src->size.h - 1);
    row0 = (const uint8_t *) src->base + (size_t) y0 * src->rowbytes;
    row1 = (const uint8_t *) src->base + (size_t) y1 * src->rowbytes;

    for (x = 0; x < size.w; x++, px += 4)
    {
      x0 = cols[x].x0;
      x1 = cols[x].x1;
      wx = cols[x].wx;

      for (c = 0; c < 4; c++)
      {
        if (linear && c != other)
        {
          v = resize_lerp(g.decode[row0[x0 + c]],
                          g.decode[row0[x1 + c]],
                          g.decode[row1[x0 + c]],
                          g.decode[row1[x1 + c]], wx, wy);
          /* v is 0..LINEAR_MAX, so the scaling fits 32 bits, where
           * dividing by the constant needs no division call */
          px[c] = g.encode[MIN(((unsigned int) v * (ENCODE_SIZE - 1) +
                                LINEAR_MAX / 2) / LINEAR_MAX,
                               ENCODE_SIZE - 1)];
        }
        else
        {
          px[c] = (uint8_t) resize_lerp(row0[x0 + c], row0[x1 + c],
                                        row1[x0 + c], row1[x1 + c],
                                        wx, wy);
        }
      }
    }
  }

  free(cols);

  *out = dst;

  return result_OK;
}
