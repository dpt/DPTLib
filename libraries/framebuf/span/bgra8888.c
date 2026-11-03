/* framebuf/span/bgra8888.c */

#include "base/utils.h"

#include "utils/maths.h"

#include "framebuf/pixelfmt.h"

#include "framebuf/span.h"

#include "framebuf/span-bgra8888.h"

#include "all8888.h"

/* Composite the colour of src2 at coverage alpha over src1, Porter-Duff
 * source over destination on straight (not premultiplied) alpha. src2's own
 * alpha byte is ignored: alpha is its coverage. */
static pixelfmt_bgra8888_t blend_pix(pixelfmt_bgra8888_t src1,
                                     pixelfmt_bgra8888_t src2,
                                     int                 alpha)
{
  unsigned int a1;
  unsigned int w1, w2, d;
  unsigned int r;
  unsigned int out;
  int          shift;

  if (alpha <= 0)
    return src1;
  if (alpha >= 255)
    return src2 | 0xFF000000u;

  a1 = src1 >> 24;
  if (a1 == 0)
    /* over nothing: the colour is src2's and the coverage is the alpha */
    return (src2 & 0x00FFFFFFu) | ((unsigned int) alpha << 24);

  w2 = (unsigned int) alpha * 255;
  w1 = a1 * (255 - (unsigned int) alpha);

  if (a1 == 255)
  {
    /* over opaque: d is 255 * 255, a constant the compiler divides by
     * without a division */
    out = 0xFF000000u;
    for (shift = 0; shift < 24; shift += 8)
      out |= ((((src1 >> shift) & 0xFF) * w1 +
               ((src2 >> shift) & 0xFF) * w2 + 65025 / 2) / 65025) << shift;
    return out;
  }

  /* one division per pixel, not three: div_recip does the channels */
  d = w1 + w2; /* the result's alpha, times 255 */
  r = DIV_RECIP(d);

  out = ((d + 127) / 255) << 24;
  for (shift = 0; shift < 24; shift += 8)
    out |= div_recip(((src1 >> shift) & 0xFF) * w1 +
                     ((src2 >> shift) & 0xFF) * w2 + d / 2, d, r) << shift;

  return out;
}

static void span_bgra8888_blendconst(void       *vdst,
                                     const void *vsrc1,
                                     const void *vsrc2,
                                     int         length,
                                     int         alpha,
                                     const void *context)
{
  pixelfmt_bgra8888_t       *dst;
  const pixelfmt_bgra8888_t *src1;
  const pixelfmt_bgra8888_t *src2;

  NOT_USED(context);

  dst  = vdst;
  src1 = vsrc1;
  src2 = vsrc2;

  while (length--)
    *dst++ = blend_pix(*src1++, *src2++, alpha);
}

static void span_bgra8888_blendarray(void                *vdst,
                                     const void          *vsrc1,
                                     const void          *vsrc2,
                                     int                  length,
                                     const unsigned char *alphas)
{
  pixelfmt_bgra8888_t       *dst;
  const pixelfmt_bgra8888_t *src1;
  const pixelfmt_bgra8888_t *src2;

  dst  = vdst;
  src1 = vsrc1;
  src2 = vsrc2;

  while (length--)
    *dst++ = blend_pix(*src1++, *src2++, *alphas++);
}

const span_t span_bgra8888 =
{
  pixelfmt_bgra8888,
  span_all8888_copy,
  span_all8888_fill,
  span_bgra8888_blendconst,
  span_bgra8888_blendarray,
};
