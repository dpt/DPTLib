/* framebuf/span/all16bpp-generic.c -- alpha blending common to all 16bpp
 * (555x/x555/565) formats */

#include "base/utils.h"

#if !defined(RED_SHIFT) || !defined(GREEN_SHIFT) || !defined(BLUE_SHIFT)
#error A required shift definition is missing.
#endif
#if !defined(RED_MASK) || !defined(GREEN_MASK) || !defined(BLUE_MASK)
#error A required mask definition is missing.
#endif

/* Blend specified pixels. Channel fields are not byte-aligned (5 or 6 bits
 * wide), but the linear interpolation below works on the raw field value
 * regardless of its width -- only the final re-pack needs the field's own
 * shift and mask. */
#define SPAN_ALL16BPP_BLEND_PIX(fmt, src1, src2, alpha, dst)                   \
{                                                                              \
  fmt r1, g1, b1;                                                              \
  fmt r2, g2, b2;                                                              \
                                                                               \
  if (alpha == 0)                                                             \
    dst = src1;                                                               \
  else if (alpha == 255)                                                      \
    dst = src2;                                                               \
  else                                                                        \
  {                                                                           \
    r1 = (src1 & RED_MASK) >> RED_SHIFT;                                      \
    r2 = (src2 & RED_MASK) >> RED_SHIFT;                                      \
    r1 = (r1 * (256 - alpha) + r2 * alpha) >> 8;                              \
                                                                               \
    g1 = (src1 & GREEN_MASK) >> GREEN_SHIFT;                                  \
    g2 = (src2 & GREEN_MASK) >> GREEN_SHIFT;                                  \
    g1 = (g1 * (256 - alpha) + g2 * alpha) >> 8;                              \
                                                                               \
    b1 = (src1 & BLUE_MASK) >> BLUE_SHIFT;                                    \
    b2 = (src2 & BLUE_MASK) >> BLUE_SHIFT;                                    \
    b1 = (b1 * (256 - alpha) + b2 * alpha) >> 8;                              \
                                                                               \
    dst = (fmt) ((r1 << RED_SHIFT) | (g1 << GREEN_SHIFT) | (b1 << BLUE_SHIFT)); \
  }                                                                           \
}

/* Alpha adjust, shared with all8888-generic.c's rounding convention. */
#define SPAN_ALL16BPP_BLEND_PIX_PRE(alpha) \
alpha += alpha >= 128;

/* Blend run of pixels with constant alpha. */
#define SPAN_ALL16BPP_BLEND_CONST(func, fmt)                                  \
static void func(void       *vdst,                                            \
                 const void *vsrc1,                                           \
                 const void *vsrc2,                                           \
                 int         length,                                         \
                 int         alpha,                                          \
                 const void *context)                                        \
{                                                                             \
    fmt       *pdst  = vdst;                                                 \
    const fmt *psrc1 = vsrc1;                                                \
    const fmt *psrc2 = vsrc2;                                                \
                                                                               \
    NOT_USED(context);                                                       \
                                                                               \
    SPAN_ALL16BPP_BLEND_PIX_PRE(alpha)                                        \
                                                                               \
    while (length--)                                                         \
    {                                                                        \
        SPAN_ALL16BPP_BLEND_PIX(fmt, *psrc1, *psrc2, alpha, *pdst)           \
                                                                               \
        pdst++;                                                              \
        psrc1++;                                                             \
        psrc2++;                                                             \
    }                                                                        \
}

/* Blend run of pixels with variable alpha. */
#define SPAN_ALL16BPP_BLEND_ARRAY(func, fmt)                                  \
static void func(void                *vdst,                                   \
                 const void          *vsrc1,                                  \
                 const void          *vsrc2,                                  \
                 int                  length,                                \
                 const unsigned char *palphas)                               \
{                                                                             \
    fmt       *pdst  = vdst;                                                 \
    const fmt *psrc1 = vsrc1;                                                \
    const fmt *psrc2 = vsrc2;                                                \
                                                                               \
    while (length--)                                                        \
    {                                                                        \
        int alpha = *palphas;                                                \
                                                                               \
        SPAN_ALL16BPP_BLEND_PIX_PRE(alpha)                                    \
                                                                               \
        SPAN_ALL16BPP_BLEND_PIX(fmt, *psrc1, *psrc2, alpha, *pdst)           \
                                                                               \
        pdst++;                                                              \
        psrc1++;                                                             \
        psrc2++;                                                             \
        palphas++;                                                           \
    }                                                                        \
}
