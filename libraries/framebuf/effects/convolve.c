/* framebuf/effects/convolve.c -- separable convolution of 32bpp bitmaps */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

/* Each stage of the kernel is held as a pair of three-weight lookup tables
 * packed into 10-bit fields, so that a stage sums three taps with one add. */

#define MASK   0x3FF
#define ROUNDD 1
#define PACK(A,B,C) (((A) << 20) + ((B) << 10) + (C))
#define INT(A) ((int) ((A) * 262144.0f + 32768.0f) >> 16)

/* ----------------------------------------------------------------------- */

typedef uint32_t pixel_t;

/* Controls the biggest size of blur achievable.
 * 1 is a 5-pt kernel. 16 is a 95-pt kernel. N gives an (N*6-1)-pt kernel. */
#define MAXLUTS 16

struct convolve_lut
{
  unsigned int lut[MAXLUTS][256];
  int          bias;
  int          stages;
};

/* ----------------------------------------------------------------------- */

result_t effects__convolve_init(const float     *kernel,
                                int              n,
                                convolve_lut_t **new_lut)
{
  convolve_lut_t *lut;
  int             s;
  int             k;

  if (n < 1 || (n + 2) / 3 > MAXLUTS)
    return result_BAD_ARG;

  lut = malloc(sizeof(*lut));
  if (lut == NULL)
    return result_OOM;

  lut->bias = 0;

  for (k = s = 0; k < n; s++)
  {
    float         k1, k2, k3;
    int           b1, b2, b3;
    unsigned int *tab;
    int           i;

    k1 = (k < n) ? kernel[k++] : 0.0f;
    k2 = (k < n) ? kernel[k++] : 0.0f;
    k3 = (k < n) ? kernel[k++] : 0.0f;
    if (k <= 3)
      k1 *= 0.5f;

    b1 = b2 = b3 = 0;
    if (k1 < 0) b1 = (int) (-k1 * 1024.0f);
    if (k2 < 0) b2 = (int) (-k2 * 1024.0f);
    if (k3 < 0) b3 = (int) (-k3 * 1024.0f);

    lut->bias += 2 * (b1 + b2 + b3);

    tab = lut->lut[s];

    for (i = 0; i < 256; i++)
    {
      tab[i] = PACK(INT((float) i * k3) + b3,
                    INT((float) i * k2) + b2 + ROUNDD,
                    INT((float) i * k1) + b1);
    }
  }

  lut->stages = s;

  *new_lut = lut;

  return result_OK;
}

void effects__convolve_destroy(convolve_lut_t *lut)
{
  free(lut);
}

/* ----------------------------------------------------------------------- */

/* Channel extraction and recombination, relative to the colour channels'
 * lowest bit `cs`. Which channel is which does not matter here as all three
 * are treated alike. */

#define CH0(PX) (((PX) >> cs) & 0xff)
#define CH1(PX) (((PX) >> (cs + 8)) & 0xff)
#define CH2(PX) (((PX) >> (cs + 16)) & 0xff)

#define FUSE(R,G,B) (((uint32_t) (R) << cs) | \
                     ((uint32_t) (G) << (cs + 8)) | \
                     ((uint32_t) (B) << (cs + 16)))

typedef void convolve_888_fn(const convolve_lut_t *lut,
                             const pixel_t    *ip,
                             pixel_t          *op,
                             int                 len,
                             int                 bias,
                             int                 stride,
                                unsigned int        cs,
                                unsigned int        amask);

static void convolve_888_1stage(const convolve_lut_t *lut,
                                const pixel_t        *ip,
                                pixel_t              *op,
                                int                   len,
                                int                   bias,
                                int                   stride,
                                unsigned int          cs,
                                unsigned int          amask)
{
  const unsigned int *lut0;
  unsigned int        fwd0_r;
  unsigned int        rev0_r;
  unsigned int        fwd0_g;
  unsigned int        rev0_g;
  unsigned int        fwd0_b;
  unsigned int        rev0_b;

  lut0 = lut->lut[0];

  ip += 2; /* ip[0] is centre pixel */

  /* See the g8 version of the code for a simpler equivalent */

  fwd0_r = (lut0[CH0(ip[-2])] >> 10) + lut0[CH0(ip[-1])];
  fwd0_g = (lut0[CH1(ip[-2])] >> 10) + lut0[CH1(ip[-1])];
  fwd0_b = (lut0[CH2(ip[-2])] >> 10) + lut0[CH2(ip[-1])];

  rev0_r = (lut0[CH0(ip[ 0])] << 10) + lut0[CH0(ip[ 1])];
  rev0_g = (lut0[CH1(ip[ 0])] << 10) + lut0[CH1(ip[ 1])];
  rev0_b = (lut0[CH2(ip[ 0])] << 10) + lut0[CH2(ip[ 1])];

  while (len--)
  {
    int val_r, val_g, val_b;

    fwd0_r = (fwd0_r >> 10) + lut0[CH0(ip[0])];
    fwd0_g = (fwd0_g >> 10) + lut0[CH1(ip[0])];
    fwd0_b = (fwd0_b >> 10) + lut0[CH2(ip[0])];

    rev0_r = (rev0_r << 10) + lut0[CH0(ip[2])];
    rev0_g = (rev0_g << 10) + lut0[CH1(ip[2])];
    rev0_b = (rev0_b << 10) + lut0[CH2(ip[2])];

    val_r = ((int) ((fwd0_r & MASK) + ((rev0_r >> 20) & MASK)) - bias) >> 2;
    val_r = CLAMP(val_r, 0, 255);

    val_g = ((int) ((fwd0_g & MASK) + ((rev0_g >> 20) & MASK)) - bias) >> 2;
    val_g = CLAMP(val_g, 0, 255);

    val_b = ((int) ((fwd0_b & MASK) + ((rev0_b >> 20) & MASK)) - bias) >> 2;
    val_b = CLAMP(val_b, 0, 255);

    *op = FUSE(val_r, val_g, val_b) | (ip[0] & amask);

    ip++;

    op += stride;
  }
}

static void convolve_888_2stage(const convolve_lut_t *lut,
                                const pixel_t        *ip,
                                pixel_t              *op,
                                int                   len,
                                int                   bias,
                                int                   stride,
                                unsigned int          cs,
                                unsigned int          amask)
{
  const unsigned int *lut0;
  const unsigned int *lut1;
  unsigned int        fwd0_r, fwd1_r;
  unsigned int        rev0_r, rev1_r;
  unsigned int        fwd0_g, fwd1_g;
  unsigned int        rev0_g, rev1_g;
  unsigned int        fwd0_b, fwd1_b;
  unsigned int        rev0_b, rev1_b;

  lut0 = lut->lut[0];
  lut1 = lut->lut[1];

  ip += 5; /* ip[0] is centre pixel */

  fwd0_r = (lut0[CH0(ip[-2])] >> 10) + lut0[CH0(ip[-1])];
  fwd0_g = (lut0[CH1(ip[-2])] >> 10) + lut0[CH1(ip[-1])];
  fwd0_b = (lut0[CH2(ip[-2])] >> 10) + lut0[CH2(ip[-1])];

  rev0_r = (lut0[CH0(ip[ 0])] << 10) + lut0[CH0(ip[ 1])];
  rev0_g = (lut0[CH1(ip[ 0])] << 10) + lut0[CH1(ip[ 1])];
  rev0_b = (lut0[CH2(ip[ 0])] << 10) + lut0[CH2(ip[ 1])];

  fwd1_r = (lut1[CH0(ip[-5])] >> 10) + lut1[CH0(ip[-4])];
  fwd1_g = (lut1[CH1(ip[-5])] >> 10) + lut1[CH1(ip[-4])];
  fwd1_b = (lut1[CH2(ip[-5])] >> 10) + lut1[CH2(ip[-4])];

  rev1_r = (lut1[CH0(ip[ 3])] << 10) + lut1[CH0(ip[ 4])];
  rev1_g = (lut1[CH1(ip[ 3])] << 10) + lut1[CH1(ip[ 4])];
  rev1_b = (lut1[CH2(ip[ 3])] << 10) + lut1[CH2(ip[ 4])];

  while (len--)
  {
    int val_r, val_g, val_b;

    fwd0_r = (fwd0_r >> 10) + lut0[CH0(ip[ 0])];
    fwd0_g = (fwd0_g >> 10) + lut0[CH1(ip[ 0])];
    fwd0_b = (fwd0_b >> 10) + lut0[CH2(ip[ 0])];

    rev0_r = (rev0_r << 10) + lut0[CH0(ip[ 2])];
    rev0_g = (rev0_g << 10) + lut0[CH1(ip[ 2])];
    rev0_b = (rev0_b << 10) + lut0[CH2(ip[ 2])];

    fwd1_r = (fwd1_r >> 10) + lut1[CH0(ip[-3])];
    fwd1_g = (fwd1_g >> 10) + lut1[CH1(ip[-3])];
    fwd1_b = (fwd1_b >> 10) + lut1[CH2(ip[-3])];

    rev1_r = (rev1_r << 10) + lut1[CH0(ip[ 5])];
    rev1_g = (rev1_g << 10) + lut1[CH1(ip[ 5])];
    rev1_b = (rev1_b << 10) + lut1[CH2(ip[ 5])];

    val_r = ((int) ((fwd0_r & MASK) + ((rev0_r >> 20) & MASK) +
             (fwd1_r & MASK) + ((rev1_r >> 20) & MASK)) - bias) >> 2;
    val_r = CLAMP(val_r, 0, 255);

    val_g = ((int) ((fwd0_g & MASK) + ((rev0_g >> 20) & MASK) +
             (fwd1_g & MASK) + ((rev1_g >> 20) & MASK)) - bias) >> 2;
    val_g = CLAMP(val_g, 0, 255);

    val_b = ((int) ((fwd0_b & MASK) + ((rev0_b >> 20) & MASK) +
             (fwd1_b & MASK) + ((rev1_b >> 20) & MASK)) - bias) >> 2;
    val_b = CLAMP(val_b, 0, 255);

    *op = FUSE(val_r, val_g, val_b) | (ip[0] & amask);

    ip++;

    op += stride;
  }
}

static void convolve_888_3stage(const convolve_lut_t *lut,
                                const pixel_t        *ip,
                                pixel_t              *op,
                                int                   len,
                                int                   bias,
                                int                   stride,
                                unsigned int          cs,
                                unsigned int          amask)
{
  const unsigned int *lut0;
  const unsigned int *lut1;
  const unsigned int *lut2;
  unsigned int        fwd0_r, fwd1_r, fwd2_r;
  unsigned int        rev0_r, rev1_r, rev2_r;
  unsigned int        fwd0_g, fwd1_g, fwd2_g;
  unsigned int        rev0_g, rev1_g, rev2_g;
  unsigned int        fwd0_b, fwd1_b, fwd2_b;
  unsigned int        rev0_b, rev1_b, rev2_b;

  lut0 = lut->lut[0];
  lut1 = lut->lut[1];
  lut2 = lut->lut[2];

  ip += 8; /* ip[0] is centre pixel */

  fwd0_r = (lut0[CH0(ip[-2])] >> 10) + lut0[CH0(ip[-1])];
  fwd0_g = (lut0[CH1(ip[-2])] >> 10) + lut0[CH1(ip[-1])];
  fwd0_b = (lut0[CH2(ip[-2])] >> 10) + lut0[CH2(ip[-1])];

  rev0_r = (lut0[CH0(ip[ 0])] << 10) + lut0[CH0(ip[ 1])];
  rev0_g = (lut0[CH1(ip[ 0])] << 10) + lut0[CH1(ip[ 1])];
  rev0_b = (lut0[CH2(ip[ 0])] << 10) + lut0[CH2(ip[ 1])];

  fwd1_r = (lut1[CH0(ip[-5])] >> 10) + lut1[CH0(ip[-4])];
  fwd1_g = (lut1[CH1(ip[-5])] >> 10) + lut1[CH1(ip[-4])];
  fwd1_b = (lut1[CH2(ip[-5])] >> 10) + lut1[CH2(ip[-4])];

  rev1_r = (lut1[CH0(ip[ 3])] << 10) + lut1[CH0(ip[ 4])];
  rev1_g = (lut1[CH1(ip[ 3])] << 10) + lut1[CH1(ip[ 4])];
  rev1_b = (lut1[CH2(ip[ 3])] << 10) + lut1[CH2(ip[ 4])];

  fwd2_r = (lut2[CH0(ip[-8])] >> 10) + lut2[CH0(ip[-7])];
  fwd2_g = (lut2[CH1(ip[-8])] >> 10) + lut2[CH1(ip[-7])];
  fwd2_b = (lut2[CH2(ip[-8])] >> 10) + lut2[CH2(ip[-7])];

  rev2_r = (lut2[CH0(ip[ 6])] << 10) + lut2[CH0(ip[ 7])];
  rev2_g = (lut2[CH1(ip[ 6])] << 10) + lut2[CH1(ip[ 7])];
  rev2_b = (lut2[CH2(ip[ 6])] << 10) + lut2[CH2(ip[ 7])];

  while (len--)
  {
    int val_r, val_g, val_b;

    fwd0_r = (fwd0_r >> 10) + lut0[CH0(ip[ 0])];
    fwd0_g = (fwd0_g >> 10) + lut0[CH1(ip[ 0])];
    fwd0_b = (fwd0_b >> 10) + lut0[CH2(ip[ 0])];

    rev0_r = (rev0_r << 10) + lut0[CH0(ip[ 2])];
    rev0_g = (rev0_g << 10) + lut0[CH1(ip[ 2])];
    rev0_b = (rev0_b << 10) + lut0[CH2(ip[ 2])];

    fwd1_r = (fwd1_r >> 10) + lut1[CH0(ip[-3])];
    fwd1_g = (fwd1_g >> 10) + lut1[CH1(ip[-3])];
    fwd1_b = (fwd1_b >> 10) + lut1[CH2(ip[-3])];

    rev1_r = (rev1_r << 10) + lut1[CH0(ip[ 5])];
    rev1_g = (rev1_g << 10) + lut1[CH1(ip[ 5])];
    rev1_b = (rev1_b << 10) + lut1[CH2(ip[ 5])];

    fwd2_r = (fwd2_r >> 10) + lut2[CH0(ip[-6])];
    fwd2_g = (fwd2_g >> 10) + lut2[CH1(ip[-6])];
    fwd2_b = (fwd2_b >> 10) + lut2[CH2(ip[-6])];

    rev2_r = (rev2_r << 10) + lut2[CH0(ip[ 8])];
    rev2_g = (rev2_g << 10) + lut2[CH1(ip[ 8])];
    rev2_b = (rev2_b << 10) + lut2[CH2(ip[ 8])];

    val_r = ((int) ((fwd0_r & MASK) + ((rev0_r >> 20) & MASK) +
             (fwd1_r & MASK) + ((rev1_r >> 20) & MASK) +
             (fwd2_r & MASK) + ((rev2_r >> 20) & MASK)) - bias) >> 2;
    val_r = CLAMP(val_r, 0, 255);

    val_g = ((int) ((fwd0_g & MASK) + ((rev0_g >> 20) & MASK) +
             (fwd1_g & MASK) + ((rev1_g >> 20) & MASK) +
             (fwd2_g & MASK) + ((rev2_g >> 20) & MASK)) - bias) >> 2;
    val_g = CLAMP(val_g, 0, 255);

    val_b = ((int) ((fwd0_b & MASK) + ((rev0_b >> 20) & MASK) +
             (fwd1_b & MASK) + ((rev1_b >> 20) & MASK) +
             (fwd2_b & MASK) + ((rev2_b >> 20) & MASK)) - bias) >> 2;
    val_b = CLAMP(val_b, 0, 255);

    *op = FUSE(val_r, val_g, val_b) | (ip[0] & amask);

    ip++;

    op += stride;
  }
}

static void convolve_888_Nstage(const convolve_lut_t *lut,
                                const pixel_t        *ip,
                                pixel_t              *op,
                                int                   len,
                                int                   bias,
                                int                   stride,
                                unsigned int          cs,
                                unsigned int          amask)
{
  int                 stages;
  int                 i;
  const unsigned int *lutN[MAXLUTS];
  unsigned int        fwd_r[MAXLUTS];
  unsigned int        rev_r[MAXLUTS];
  unsigned int        fwd_g[MAXLUTS];
  unsigned int        rev_g[MAXLUTS];
  unsigned int        fwd_b[MAXLUTS];
  unsigned int        rev_b[MAXLUTS];

  stages = lut->stages;

  for (i = 0; i < stages; i++)
    lutN[i] = lut->lut[i];

  ip += 3 * stages - 1; /* ip[0] is centre pixel */

  for (i = 0; i < stages; i++)
  {
    int j,k;

    j = -3 * i - 2;
    k = j + 1;

    fwd_r[i] = (lutN[i][CH0(ip[j])] >> 10) + lutN[i][CH0(ip[k])];
    fwd_g[i] = (lutN[i][CH1(ip[j])] >> 10) + lutN[i][CH1(ip[k])];
    fwd_b[i] = (lutN[i][CH2(ip[j])] >> 10) + lutN[i][CH2(ip[k])];

    j = 3 * i;
    k = j + 1;

    rev_r[i] = (lutN[i][CH0(ip[j])] << 10) + lutN[i][CH0(ip[k])];
    rev_g[i] = (lutN[i][CH1(ip[j])] << 10) + lutN[i][CH1(ip[k])];
    rev_b[i] = (lutN[i][CH2(ip[j])] << 10) + lutN[i][CH2(ip[k])];
  }

  while (len--)
  {
    int val_r, val_g, val_b;

    for (i = 0; i < stages; i++)
    {
      int j;

      j = -3 * i;

      fwd_r[i] = (fwd_r[i] >> 10) + lutN[i][CH0(ip[j])];
      fwd_g[i] = (fwd_g[i] >> 10) + lutN[i][CH1(ip[j])];
      fwd_b[i] = (fwd_b[i] >> 10) + lutN[i][CH2(ip[j])];

      j = 3 * i + 2;

      rev_r[i] = (rev_r[i] << 10) + lutN[i][CH0(ip[j])];
      rev_g[i] = (rev_g[i] << 10) + lutN[i][CH1(ip[j])];
      rev_b[i] = (rev_b[i] << 10) + lutN[i][CH2(ip[j])];
    }

    val_r = 0;
    val_g = 0;
    val_b = 0;

    for (i = 0; i < stages; i++)
    {
      val_r += (fwd_r[i] & MASK) + ((rev_r[i] >> 20) & MASK);
      val_g += (fwd_g[i] & MASK) + ((rev_g[i] >> 20) & MASK);
      val_b += (fwd_b[i] & MASK) + ((rev_b[i] >> 20) & MASK);
    }

    val_r = ((int) (val_r) - bias) >> 2;
    val_g = ((int) (val_g) - bias) >> 2;
    val_b = ((int) (val_b) - bias) >> 2;

    val_r = CLAMP(val_r, 0, 255);
    val_g = CLAMP(val_g, 0, 255);
    val_b = CLAMP(val_b, 0, 255);

    *op = FUSE(val_r, val_g, val_b) | (ip[0] & amask);

    ip++;

    op += stride;
  }
}

static void convolve_row(const convolve_lut_t *lut,
                         const pixel_t        *src,
                         pixel_t              *dst,
                         int                   len,
                         int                   srcstride,
                         int                   dststride,
                         pixel_t              *buf,
                         unsigned int          cs,
                         unsigned int          amask)
{
  int              padlen;
  const pixel_t   *p1;
  pixel_t         *p2;
  int              x;
  convolve_888_fn *fn;

  /* buffer and pad the input */

  padlen = 3 * lut->stages - 1;
  p1 = src;
  p2 = buf;
  for (x = 0; x < padlen; x++)
    *p2++ = *p1;
  for (x = 0; x < len; x++)
  {
    *p2++ = *p1;
    p1 += srcstride;
  }
  p1 -= srcstride;
  for (x = 0; x < padlen; x++)
    *p2++ = *p1;

  switch (lut->stages)
  {
  case 1: /* 5-pt kernel */
    fn = convolve_888_1stage;
    break;
  case 2: /* 11-pt kernel */
    fn = convolve_888_2stage;
    break;
  case 3: /* 17-pt kernel */
    fn = convolve_888_3stage;
    break;
  default: /* (stages*6-1)-pt kernels */
    fn = convolve_888_Nstage;
    break;
  }

  fn(lut, buf, dst, len, lut->bias, dststride, cs, amask);
}

/* ----------------------------------------------------------------------- */

result_t effects__convolve(const convolve_lut_t *lut,
                           const bitmap_t       *src,
                           bitmap_t             *dst)
{
  result_t         rc;
  effects_layout_t layout;
  int              width, height;
  int              maxdim;
  pixel_t         *row;
  unsigned int     cs;
  unsigned int     amask;
  int              dststride;
  int              x, y;

  rc = effects__check(src, dst, &layout);
  if (rc != result_OK)
    return rc;

  width  = src->size.w;
  height = src->size.h;

  if (width == 0 || height == 0)
    return result_OK;

  cs        = (layout.a == 0) ? 8 : 0;
  amask     = EFFECTS_ALPHA_MASK(&layout);
  dststride = dst->rowbytes / 4;

  /* Allow for the maximum dimension of the bitmap,
   * plus padding at either end. */
  maxdim = MAX(width, height) + (3 * lut->stages - 1) * 2;

  row = malloc((size_t) maxdim * sizeof(*row));
  if (row == NULL)
    return result_OOM;

  for (y = 0; y < height; y++) /* src -> dst */
  {
    convolve_row(lut, EFFECTS_ROW(src, y), EFFECTS_ROW(dst, y), width,
                 1, 1, row, cs, amask);
  }

  for (x = 0; x < width; x++) /* dst -> dst */
  {
    pixel_t *col;

    col = (pixel_t *) dst->base + x;

    convolve_row(lut, col, col, height, dststride, dststride, row, cs, amask);
  }

  free(row);

  return result_OK;
}
