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
 * shift and mask. Selects, not branches, pick the unblended cases so that
 * the loops vectorise. */
#define SPAN_ALL16BPP_BLEND_PIX(fmt, src1, src2, alpha, dst)                   \
{                                                                              \
  fmt          s1, s2;                                                         \
  unsigned int r, g, b;                                                        \
                                                                               \
  s1 = (src1);                                                                 \
  s2 = (src2);                                                                 \
  r  = ((((s1 & RED_MASK) >> RED_SHIFT) * (256 - alpha) +                      \
         ((s2 & RED_MASK) >> RED_SHIFT) * alpha) >> 8);                        \
  g  = ((((s1 & GREEN_MASK) >> GREEN_SHIFT) * (256 - alpha) +                  \
         ((s2 & GREEN_MASK) >> GREEN_SHIFT) * alpha) >> 8);                    \
  b  = ((((s1 & BLUE_MASK) >> BLUE_SHIFT) * (256 - alpha) +                    \
         ((s2 & BLUE_MASK) >> BLUE_SHIFT) * alpha) >> 8);                      \
  dst = alpha == 0 ? s1 : alpha == 255 ? s2 :                                  \
        (fmt) ((r << RED_SHIFT) | (g << GREEN_SHIFT) | (b << BLUE_SHIFT));     \
}

/* Alpha adjust, shared with all8888-generic.c's rounding convention. */
#define SPAN_ALL16BPP_BLEND_PIX_PRE(alpha) \
alpha += alpha >= 128;

/* Each blend below has a loop of its own for blending in place, src1 being
 * dst: one through both pointers would not vectorise, as the compiler's
 * overlap check sees them alias. */

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
    if (pdst == psrc1)                                                        \
    {                                                                         \
        while (length--)                                                      \
        {                                                                     \
            SPAN_ALL16BPP_BLEND_PIX(fmt, *pdst, *psrc2, alpha, *pdst)         \
            pdst++;                                                           \
            psrc2++;                                                          \
        }                                                                     \
        return;                                                               \
    }                                                                         \
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
    if (pdst == psrc1)                                                       \
    {                                                                        \
        while (length--)                                                     \
        {                                                                    \
            int alpha = *palphas++;                                          \
                                                                               \
            SPAN_ALL16BPP_BLEND_PIX_PRE(alpha)                                \
            SPAN_ALL16BPP_BLEND_PIX(fmt, *pdst, *psrc2, alpha, *pdst)        \
            pdst++;                                                          \
            psrc2++;                                                         \
        }                                                                    \
        return;                                                              \
    }                                                                        \
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
