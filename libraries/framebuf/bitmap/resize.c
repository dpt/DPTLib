/* resize.c -- resample a 32bpp bitmap to a new size */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/utils.h"
#include "framebuf/bitmap.h"

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

#define LINEAR_MAX   65535
#define ENCODE_SIZE  4096 /* entries in the linear-to-sRGB table */

static uint16_t resize_decode[256];
static uint8_t  resize_encode[ENCODE_SIZE];
static int      resize_tables_ready;

static void resize_make_tables(void)
{
  int    i;
  double c;

  if (resize_tables_ready)
    return;

  for (i = 0; i < 256; i++)
  {
    c = i / 255.0;
    c = c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
    resize_decode[i] = (uint16_t) (c * LINEAR_MAX + 0.5);
  }

  for (i = 0; i < ENCODE_SIZE; i++)
  {
    c = i / (double) (ENCODE_SIZE - 1);
    c = c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1 / 2.4) - 0.055;
    resize_encode[i] = (uint8_t) (c * 255 + 0.5);
  }

  resize_tables_ready = 1;
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

/* Box filter: each destination pixel is the area-weighted average of the
 * source rectangle it covers. */
static void resize_box(const bitmap_t *src,
                       uint8_t        *base,
                       size2d_t        size,
                       int             linear,
                       int             other)
{
  int            x, y, c;
  int            sx, sy;
  int            sx0, sx1, sy0, sy1;
  double         fx0, fx1, fy0, fy1;
  double         wx, wy, w, total;
  double         acc[4];
  const uint8_t *sp;
  uint8_t       *px;
  int            v;

  for (y = 0; y < size.h; y++)
  {
    fy0 = (double) y * src->size.h / size.h;
    fy1 = (double) (y + 1) * src->size.h / size.h;
    sy0 = (int) fy0;
    sy1 = MIN((int) ceil(fy1), src->size.h);
    px  = base + (size_t) y * size.w * 4;

    for (x = 0; x < size.w; x++, px += 4)
    {
      fx0 = (double) x * src->size.w / size.w;
      fx1 = (double) (x + 1) * src->size.w / size.w;
      sx0 = (int) fx0;
      sx1 = MIN((int) ceil(fx1), src->size.w);

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
            acc[c] += w * (linear && c != other ? resize_decode[sp[c]]
                                                : sp[c]);
        }
      }

      for (c = 0; c < 4; c++)
      {
        acc[c] /= total;
        if (linear && c != other)
        {
          v = (int) (acc[c] * (ENCODE_SIZE - 1) / LINEAR_MAX + 0.5);
          px[c] = resize_encode[MIN(v, ENCODE_SIZE - 1)];
        }
        else
        {
          px[c] = (uint8_t) (acc[c] + 0.5);
        }
      }
    }
  }
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

/* Bicubic (Catmull-Rom) over a 4x4 neighbourhood, clamped at the edges.
 * Sharper than bilinear but can overshoot, so the result is clamped. */
static void resize_cubic(const bitmap_t *src,
                         uint8_t        *base,
                         size2d_t        size,
                         int             linear,
                         int             other)
{
  int            x, y, c, i, j;
  int            ix, iy, sx, sy;
  double         fx, fy;
  double         wx[4], wy[4];
  double         acc[4], row[4];
  const uint8_t *sp;
  uint8_t       *px;
  int            v, max;

  for (y = 0; y < size.h; y++)
  {
    fy = (y + 0.5) * src->size.h / size.h - 0.5;
    iy = (int) floor(fy);
    resize_cubic_weights(fy - iy, wy);
    px = base + (size_t) y * size.w * 4;

    for (x = 0; x < size.w; x++, px += 4)
    {
      fx = (x + 0.5) * src->size.w / size.w - 0.5;
      ix = (int) floor(fx);
      resize_cubic_weights(fx - ix, wx);

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
            row[c] += wx[i] * (linear && c != other ? resize_decode[sp[c]]
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
          px[c] = resize_encode[CLAMP(v, 0, ENCODE_SIZE - 1)];
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
}

result_t bitmap_resize(const bitmap_t   *src,
                       size2d_t          size,
                       bitmap_resample_t method,
                       int               linear,
                       bitmap_t        **out)
{
  bitmap_t      *dst;
  uint8_t       *base;
  int            dstrow;
  int            x, y, c;
  int64_t        fx, fy;
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

  if (method == bitmap_resample_CUBIC)
  {
    resize_cubic(src, base, size, linear, other);
    *out = dst;
    return result_OK;
  }

  /* box only helps when shrinking; otherwise it is bilinear */
  if (method == bitmap_resample_BOX)
  {
    if (size.w < src->size.w || size.h < src->size.h)
    {
      resize_box(src, base, size, linear, other);
      *out = dst;
      return result_OK;
    }
    method = bitmap_resample_BILINEAR;
  }

  for (y = 0; y < size.h; y++)
  {
    px = base + (size_t) y * dstrow;

    if (method == bitmap_resample_NEAREST)
    {
      y0   = (int) (((int64_t) y * src->size.h) / size.h);
      row0 = (const uint8_t *) src->base + (size_t) y0 * src->rowbytes;
      for (x = 0; x < size.w; x++, px += 4)
      {
        x0 = (int) (((int64_t) x * src->size.w) / size.w);
        for (c = 0; c < 4; c++)
          px[c] = row0[x0 * 4 + c];
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
      fx = resize_centre(x, src->size.w, size.w);
      x0 = (int) (fx >> FRAC_BITS);
      wx = (int) (fx - ((int64_t) x0 * (1 << FRAC_BITS)));
      x1 = x0 + 1;
      x0 = CLAMP(x0, 0, src->size.w - 1);
      x1 = CLAMP(x1, 0, src->size.w - 1);

      for (c = 0; c < 4; c++)
      {
        if (linear && c != other)
        {
          v = resize_lerp(resize_decode[row0[x0 * 4 + c]],
                          resize_decode[row0[x1 * 4 + c]],
                          resize_decode[row1[x0 * 4 + c]],
                          resize_decode[row1[x1 * 4 + c]], wx, wy);
          px[c] = resize_encode[MIN((v * (ENCODE_SIZE - 1) +
                                     LINEAR_MAX / 2) / LINEAR_MAX,
                                    ENCODE_SIZE - 1)];
        }
        else
        {
          px[c] = (uint8_t) resize_lerp(row0[x0 * 4 + c], row0[x1 * 4 + c],
                                        row1[x0 * 4 + c], row1[x1 * 4 + c],
                                        wx, wy);
        }
      }
    }
  }

  *out = dst;

  return result_OK;
}
