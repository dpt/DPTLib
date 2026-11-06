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

/* ----- Separable filters, widened when shrinking ----- */

#define RESIZE_PI 3.14159265358979323846

/* A filter kernel and the distance from its centre at which it is cut off,
 * both in source pixels at unit scale. */
typedef struct resize_kernel
{
  double (*fn)(double x);
  double   support;
}
resize_kernel_t;

static double resize_tent(double x)
{
  x = fabs(x);

  return x < 1.0 ? 1.0 - x : 0.0;
}

static double resize_hermite(double x)
{
  x = fabs(x);

  return x < 1.0 ? (2.0 * x - 3.0) * x * x + 1.0 : 0.0;
}

/* Mitchell-Netravali's family of cubics, parameterised by B and C. */
static double resize_bicubic(double x, double b, double c)
{
  x = fabs(x);
  if (x < 1.0)
    return ((12.0 - 9.0 * b - 6.0 * c) * x * x * x +
            (-18.0 + 12.0 * b + 6.0 * c) * x * x +
            (6.0 - 2.0 * b)) / 6.0;
  if (x < 2.0)
    return ((-b - 6.0 * c) * x * x * x +
            (6.0 * b + 30.0 * c) * x * x +
            (-12.0 * b - 48.0 * c) * x +
            (8.0 * b + 24.0 * c)) / 6.0;

  return 0.0;
}

static double resize_bspline(double x)
{
  return resize_bicubic(x, 1.0, 0.0);
}

static double resize_mitchell(double x)
{
  return resize_bicubic(x, 1.0 / 3.0, 1.0 / 3.0);
}

static double resize_catmull_rom(double x)
{
  return resize_bicubic(x, 0.0, 0.5);
}

/* Standard deviation of half a pixel. */
static double resize_gaussian(double x)
{
  return exp(-2.0 * x * x);
}

static double resize_sinc(double x)
{
  return x == 0.0 ? 1.0 : sin(RESIZE_PI * x) / (RESIZE_PI * x);
}

static double resize_lanczos2(double x)
{
  x = fabs(x);

  return x < 2.0 ? resize_sinc(x) * resize_sinc(x / 2.0) : 0.0;
}

static double resize_lanczos3(double x)
{
  x = fabs(x);

  return x < 3.0 ? resize_sinc(x) * resize_sinc(x / 3.0) : 0.0;
}

/* Methods with no kernel here have their own loops. */
static const resize_kernel_t resize_kernels[bitmap_resample__LIMIT] =
{
  [bitmap_resample_BILINEAR] = { resize_tent,        1.0 },
  [bitmap_resample_BSPLINE]  = { resize_bspline,     2.0 },
  [bitmap_resample_GAUSSIAN] = { resize_gaussian,    2.0 },
  [bitmap_resample_HERMITE]  = { resize_hermite,     1.0 },
  [bitmap_resample_MITCHELL] = { resize_mitchell,    2.0 },
  [bitmap_resample_CUBIC]    = { resize_catmull_rom, 2.0 },
  [bitmap_resample_LANCZOS2] = { resize_lanczos2,    2.0 },
  [bitmap_resample_LANCZOS3] = { resize_lanczos3,    3.0 }
};

/* A destination pixel's filter taps along one axis: n weights for the source
 * pixels from first onwards. */
typedef struct resize_taps
{
  int          first;
  int          n;
  const float *w;
}
resize_taps_t;

/* Build one entry per destination pixel, in a single block for free(). When
 * shrinking, the kernel widens by the shrink factor so that every source
 * pixel contributes. Taps beyond the edges land on the edge pixels. */
static resize_taps_t *resize_make_taps(int                    srclen,
                                       int                    dstlen,
                                       const resize_kernel_t *kernel,
                                       int                   *pmaxtaps)
{
  double         ratio, scale, radius;
  int            maxtaps;
  resize_taps_t *taps;
  float         *weights;
  int            d, i;
  double         centre;
  int            lo, hi, first, last;
  float         *w;
  double         total, k;

  ratio   = (double) srclen / dstlen;
  scale   = MAX(ratio, 1.0);
  radius  = kernel->support * scale;
  maxtaps = (int) ceil(2.0 * radius) + 2;

  taps = calloc(1, (size_t) dstlen * sizeof(*taps) +
                   (size_t) dstlen * maxtaps * sizeof(*weights));
  if (taps == NULL)
    return NULL;
  weights = (float *) (taps + dstlen);

  for (d = 0; d < dstlen; d++)
  {
    centre = (d + 0.5) * ratio - 0.5;
    lo     = (int) ceil(centre - radius);
    hi     = (int) floor(centre + radius);
    first  = MAX(lo, 0);
    last   = MIN(hi, srclen - 1);
    w      = weights + (size_t) d * maxtaps;

    total = 0.0;
    for (i = lo; i <= hi; i++)
    {
      k = kernel->fn((i - centre) / scale);
      w[CLAMP(i, first, last) - first] += (float) k;
      total += k;
    }
    for (i = 0; i <= last - first; i++)
      w[i] = (float) (w[i] / total);

    taps[d].first = first;
    taps[d].n     = last - first + 1;
    taps[d].w     = w;
  }

  *pmaxtaps = maxtaps;

  return taps;
}

/* Filter source row sy along x into row, which has four lanes per pixel. */
static void resize_filter_row(const bitmap_t      *src,
                              int                  sy,
                              const resize_taps_t *taps,
                              float               *row,
                              int                  width,
                              int                  linear,
                              int                  other)
{
  const uint8_t *base;
  int            x, i, c;
  const uint8_t *sp;
  float          acc[4];
  float          w;

  base = (const uint8_t *) src->base + (size_t) sy * src->rowbytes;

  for (x = 0; x < width; x++, row += 4)
  {
    sp     = base + (size_t) taps[x].first * 4;
    acc[0] = acc[1] = acc[2] = acc[3] = 0.0f;
    for (i = 0; i < taps[x].n; i++, sp += 4)
    {
      w = taps[x].w[i];
      for (c = 0; c < 4; c++)
        acc[c] += w * (linear && c != other ? g.decode[sp[c]] : sp[c]);
    }
    for (c = 0; c < 4; c++)
      row[c] = acc[c];
  }
}

/* Separable filter: x then y, keeping just the filtered source rows the
 * current destination row needs. The kernels can overshoot, so the result is
 * clamped. The weights are worked out once, so the pixel loops do no
 * division. */
static result_t resize_filter(const bitmap_t        *src,
                              uint8_t               *base,
                              size2d_t               size,
                              const resize_kernel_t *kernel,
                              int                    linear,
                              int                    other)
{
  result_t       rc;
  int            xmax, ymax;
  resize_taps_t *xtaps, *ytaps;
  int           *slots;
  float         *rows;
  int            nslots;
  size_t         lanes, l;
  float         *acc;
  float          encode_scale;
  int            y, i, slot, sy, v;
  float         *row;
  float          w;
  uint8_t       *px;

  rc    = result_OOM;
  slots = NULL;
  rows  = NULL;
  xtaps = resize_make_taps(src->size.w, size.w, kernel, &xmax);
  ytaps = resize_make_taps(src->size.h, size.h, kernel, &ymax);
  if (xtaps == NULL || ytaps == NULL)
    goto exit;

  /* a power of two, so masking picks a source row's slot; a destination
   * row's taps span at most ymax rows, so they never share one */
  nslots = 1;
  while (nslots < ymax)
    nslots <<= 1;

  lanes = (size_t) size.w * 4;
  slots = malloc((size_t) nslots * sizeof(*slots));
  rows  = malloc(((size_t) nslots + 1) * lanes * sizeof(*rows));
  if (slots == NULL || rows == NULL)
    goto exit;

  for (i = 0; i < nslots; i++)
    slots[i] = -1;

  acc          = rows + (size_t) nslots * lanes;
  encode_scale = (float) (ENCODE_SIZE - 1) / LINEAR_MAX;

  for (y = 0; y < size.h; y++)
  {
    for (l = 0; l < lanes; l++)
      acc[l] = 0.0f;

    for (i = 0; i < ytaps[y].n; i++)
    {
      sy   = ytaps[y].first + i;
      slot = sy & (nslots - 1);
      row  = rows + (size_t) slot * lanes;
      if (slots[slot] != sy)
      {
        resize_filter_row(src, sy, xtaps, row, size.w, linear, other);
        slots[slot] = sy;
      }

      w = ytaps[y].w[i];
      for (l = 0; l < lanes; l++)
        acc[l] += w * row[l];
    }

    px = base + (size_t) y * lanes;
    for (l = 0; l < lanes; l++)
    {
      if (linear && (int) (l & 3) != other)
      {
        v     = (int) (acc[l] * encode_scale + 0.5f);
        px[l] = g.encode[CLAMP(v, 0, ENCODE_SIZE - 1)];
      }
      else
      {
        v     = (int) (acc[l] + 0.5f);
        px[l] = (uint8_t) CLAMP(v, 0, 255);
      }
    }
  }

  rc = result_OK;

exit:
  free(rows);
  free(slots);
  free(ytaps);
  free(xtaps);

  return rc;
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
  result_t               rc;
  bitmap_t              *dst;
  uint8_t               *base;
  const resize_kernel_t *kernel;
  resize_col_t          *cols;
  int                    dstrow;
  int                    x, y, c;
  int64_t                fy;
  int                    x0, x1, y0, y1;
  int                    wx, wy;
  const uint8_t         *row0, *row1;
  uint8_t               *px;
  int                    other;
  int64_t                v;

  *out = NULL;

  if (size.w <= 0 || size.h <= 0 || src->size.w <= 0 || src->size.h <= 0 ||
      (unsigned int) method >= bitmap_resample__LIMIT)
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

  /* bilinear's two taps would skip source pixels when shrinking, so it gets
   * the widened tent; enlarging keeps the fixed-point loop below */
  kernel = resize_kernels[method].fn != NULL ? &resize_kernels[method] : NULL;
  if (method == bitmap_resample_BILINEAR &&
      size.w >= src->size.w && size.h >= src->size.h)
    kernel = NULL;

  if (kernel != NULL || method == bitmap_resample_BOX)
  {
    rc = (kernel != NULL)
         ? resize_filter(src, base, size, kernel, linear, other)
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
