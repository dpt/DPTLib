/* framebuf/span/all8888-generic.c -- alpha blending common to all 8888 formats */

#include "base/utils.h"

#if !defined(RED_SHIFT) || !defined(GREEN_SHIFT) || !defined(BLUE_SHIFT) || !defined(X_SHIFT)
#error A required shift definition is missing.
#endif

/* The blend works on red and blue together, so it needs them a lane apart
 * with green between them. */
#if GREEN_SHIFT != 8 || X_SHIFT != 24 || (RED_SHIFT | BLUE_SHIFT) != 16
#error The blend expects red and blue at bits 0 and 16.
#endif

#define RB_MASK    0x00FF00FFu
#define GREEN_MASK (0xFFu << GREEN_SHIFT)
#define X_MASK     (0xFFu << X_SHIFT)

#define PASTE2(x,y) x##y
#define PASTE3(x,y,z) x##y##z

/* Alpha adjust. */
#define SPAN_ALL8888_BLEND_PIX_PRE(alpha) \
alpha += alpha >= 128;

/* Blend specified pixels. Red and blue blend in one multiply: each lane's
 * sum is at most 255 * 256, so neither carries into the next. Selects, not
 * branches, pick the unblended cases so that the loops vectorise. */
#define SPAN_ALL8888_BLEND_PIX(fmt, src1, src2, alpha, dst)                    \
{                                                                              \
  fmt s1, s2;                                                                  \
  fmt rb, g;                                                                   \
                                                                               \
  s1 = (src1);                                                                 \
  s2 = (src2);                                                                 \
  rb = (((s1 & RB_MASK) * (256 - alpha) +                                      \
         (s2 & RB_MASK) * alpha) >> 8) & RB_MASK;                              \
  g  = (((s1 & GREEN_MASK) * (256 - alpha) +                                   \
         (s2 & GREEN_MASK) * alpha) >> 8) & GREEN_MASK;                        \
  dst = alpha == 0 ? s1 : alpha == 255 ? s2 : (rb | g | X_MASK);               \
}

/* Each blend below has a loop of its own for blending in place, src1 being
 * dst: one through both pointers would not vectorise, as the compiler's
 * overlap check sees them alias. */

/* Blend run of pixels with constant alpha. */
#define SPAN_ALL8888_BLEND_CONST(func, fmt)                                    \
static void func(void       *vdst,                                             \
                 const void *vsrc1,                                            \
                 const void *vsrc2,                                            \
                 int         length,                                           \
                 int         alpha,                                            \
                 const void *context)                                          \
{                                                                              \
    fmt       *pdst  = vdst;                                                   \
    const fmt *psrc1 = vsrc1;                                                  \
    const fmt *psrc2 = vsrc2;                                                  \
                                                                               \
    NOT_USED(context);                                                        \
                                                                               \
    SPAN_ALL8888_BLEND_PIX_PRE(alpha)                                          \
                                                                               \
    if (pdst == psrc1)                                                         \
    {                                                                          \
        while (length--)                                                       \
        {                                                                      \
            SPAN_ALL8888_BLEND_PIX(fmt, *pdst, *psrc2, alpha, *pdst)           \
            pdst++;                                                            \
            psrc2++;                                                           \
        }                                                                      \
        return;                                                                \
    }                                                                          \
                                                                               \
    while (length--)                                                           \
    {                                                                          \
        SPAN_ALL8888_BLEND_PIX(fmt, *psrc1, *psrc2, alpha, *pdst)              \
                                                                               \
        pdst++;                                                                \
        psrc1++;                                                               \
        psrc2++;                                                               \
    }                                                                          \
}

/* Blend run of pixels with variable alpha. */
#define SPAN_ALL8888_BLEND_ARRAY(func, fmt)                                    \
static void func(void                *vdst,                                    \
                 const void          *vsrc1,                                   \
                 const void          *vsrc2,                                   \
                 int                  length,                                  \
                 const unsigned char *palphas)                                 \
{                                                                              \
    fmt       *pdst  = vdst;                                                   \
    const fmt *psrc1 = vsrc1;                                                  \
    const fmt *psrc2 = vsrc2;                                                  \
                                                                               \
    if (pdst == psrc1)                                                         \
    {                                                                          \
        while (length--)                                                       \
        {                                                                      \
            int alpha = *palphas++;                                            \
                                                                               \
            SPAN_ALL8888_BLEND_PIX_PRE(alpha)                                  \
            SPAN_ALL8888_BLEND_PIX(fmt, *pdst, *psrc2, alpha, *pdst)           \
            pdst++;                                                            \
            psrc2++;                                                           \
        }                                                                      \
        return;                                                                \
    }                                                                          \
                                                                               \
    while (length--)                                                           \
    {                                                                          \
        int alpha = *palphas;                                                  \
                                                                               \
        SPAN_ALL8888_BLEND_PIX_PRE(alpha)                                      \
                                                                               \
        SPAN_ALL8888_BLEND_PIX(fmt, *psrc1, *psrc2, alpha, *pdst)              \
                                                                               \
        pdst++;                                                                \
        psrc1++;                                                               \
        psrc2++;                                                               \
        palphas++;                                                             \
    }                                                                          \
}
