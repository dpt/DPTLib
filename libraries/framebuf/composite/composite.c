/* framebuf/composite/composite.c
 *
 * An implementation of Porter-Duff image compositing.
 *
 * David Thomas, 2016
 */

/* Notes
 *
 * - Deals with non-premultiplied values only.
 * - Accepts two images as input, the destination image is overwritten with
 *   the output.
 * - Degenerate (fully transparent and fully opaque) cases are handled with
 *   dedicated code for best performance (avoiding divisions).
 */

/* References
 *
 * Compositing Digital Images (T. Porter & T. Duff, 1984)
 * http://keithp.com/~keithp/porterduff/
 *
 * Image Compositing Fundamentals (Alvy Ray Smith, 1995)
 * http://www.alvyray.com/memos/CG/Microsoft/4_comp.pdf
 *
 * Porter/Duff Compositing and Blend Modes (Søren Sandmann Pedersen, 2013)
 * http://ssp.impulsetrain.com/porterduff.html
 *
 * Compositing and Blending Level 1 (W3C, 2015)
 * https://www.w3.org/TR/compositing-1/#advancedcompositing
 */

/* ----------------------------------------------------------------------- */

/* Configuration */

/* Enable this for dedicated handling of fully transparent and fully opaque
 * pixels. */
#define SPEEDUPS

/* ----------------------------------------------------------------------- */

/* Working Out
 *
 * This working out assumes premultiplied colour values.
 *
 * General form:
 *
 * newalpha = (srcalpha * fraction_a) + (dstalpha * fraction_b)
 * newcolor = (srccolor * fraction_a) + (dstcolor * fraction_b)
 *
 * Clear:
 *
 * fraction_a = 0, fraction_b = 0
 *
 * newalpha = (srcalpha * 0) + (dstalpha * 0)
 * newcolor = (srccolor * 0) + (dstcolor * 0)
 * =>
 * newalpha = 0
 * newcolor = 0
 *
 * Source:
 *
 * fraction_a = 1, fraction_b = 0
 *
 * newalpha = (srcalpha * 1) + (dstalpha * 0)
 * newcolor = (srccolor * 1) + (dstcolor * 0)
 * =>
 * newalpha = srcalpha
 * newcolor = srccolor
 *
 * Destination:
 *
 * fraction_a = 0, fraction_b = 1
 *
 * newalpha = (srcalpha * 0) + (dstalpha * 1)
 * newcolor = (srccolor * 0) + (dstcolor * 1)
 * =>
 * newalpha = dstalpha
 * newcolor = dstcolor
 *
 * Source Over:
 *
 * fraction_a = 1, fraction_b = 1 - srcalpha
 *
 * newalpha = (srcalpha * 1) + (dstalpha * (1 - srcalpha))
 * newcolor = (srccolor * 1) + (dstcolor * (1 - srcalpha))
 * =>
 * newalpha = srcalpha + dstalpha - dstalpha * srcalpha
 * newcolor = srccolor + dstcolor - dstcolor * srcalpha
 *
 * if srcalpha == 0:  newalpha = dstalpha
 *                    newcolor = srccolor + dstcolor
 * if srcalpha == 1:  newalpha = 1
 *                    newcolor = srccolor
 * if dstalpha == 0:  newalpha = srcalpha
 *                    newcolor = srccolor + dstcolor - dstcolor * srcalpha
 * if dstalpha == 1:  newalpha = 1
 *                    newcolor = srccolor + dstcolor - dstcolor * srcalpha
 *
 * But if an alpha value is zero then its associated colour must also be zero
 * (ignoring superluminous pixels). So:
 *
 * if srcalpha == 0:  newalpha = dstalpha   // no-op when result image is destination image
 *                    newcolor = dstcolor
 * if srcalpha == 1 or
 *    dstalpha == 0:  newalpha = srcalpha   // can just copy
 *                    newcolor = srccolor
 * if dstalpha == 1:  newalpha = 1          // quick blend case. can use the destination's full opacity to consider the source alpha only
 *                    newcolor = srccolor + dstcolor - dstcolor * srcalpha
 * else:              newalpha = srcalpha + dstalpha - dstalpha * srcalpha  // general case
 *                    newcolor = srccolor + dstcolor - dstcolor * srcalpha
 *
 * Destination Over:
 *
 * fraction_a = 1 - dstalpha, fraction_b = 1
 *
 * newalpha = (srcalpha * (1 - dstalpha)) + (dstalpha * 1)
 * newcolor = (srccolor * (1 - dstalpha)) + (dstcolor * 1)
 * =>
 * newalpha = srcalpha - srcalpha * dstalpha + dstalpha
 * newcolor = srccolor - srccolor * dstalpha + dstcolor
 *
 * if srcalpha == 0 or
 *    dstalpha == 1:  newalpha = dstalpha   // no-op when result image is destination image
 *                    newcolor = dstcolor
 * if dstalpha == 0:  newalpha = srcalpha   // can just copy
 *                    newcolor = srccolor
 * if srcalpha == 1:  newalpha = 1
 *                    newcolor = srccolor - srccolor * dstalpha + dstcolor
 * else:              newalpha = srcalpha - srcalpha * dstalpha + dstalpha  // general case
 *                    newcolor = srccolor - srccolor * dstalpha + dstcolor
 *
 * Source In:
 *
 * fraction_a = dstalpha, fraction_b = 0
 *
 * newalpha = (srcalpha * dstalpha) + (dstalpha * 0)
 * newcolor = (srccolor * dstalpha) + (dstcolor * 0)
 * =>
 * newalpha = srcalpha * dstalpha
 * newcolor = srccolor * dstalpha
 *
 * if srcalpha == 0 or
 *    dstalpha == 0:  newalpha = 0
 *                    newcolor = 0
 * if dstalpha == 1:  newalpha = srcalpha   // can just copy
 *                    newcolor = srccolor
 * if srcalpha == 1:  newalpha = dstalpha
 *                    newcolor = srccolor * dstalpha
 * else:              newalpha = srcalpha * dstalpha
 *                    newcolor = srccolor * dstalpha
 *
 * Destination In:
 *
 * fraction_a = 0, fraction_b = srcalpha
 *
 * newalpha = (srcalpha * 0) + (dstalpha * srcalpha)
 * newcolor = (srccolor * 0) + (dstcolor * srcalpha)
 * =>
 * newalpha = dstalpha * srcalpha
 * newcolor = dstcolor * srcalpha
 *
 * if srcalpha == 0 or
 *    dstalpha == 0:  newalpha = 0
 *                    newcolor = 0
 * if srcalpha == 1:  newalpha = dstalpha   // no-op when result image is destination image
 *                    newcolor = dstcolor
 * if dstalpha == 1:  newalpha = srcalpha
 *                    newcolor = dstcolor * srcalpha
 * else:              newalpha = dstalpha * srcalpha
 *                    newcolor = dstcolor * srcalpha
 *
 * Source Out:
 *
 * fraction_a = 1 - dstalpha, fraction_b = 0
 *
 * newalpha = (srcalpha * (1 - dstalpha)) + (dstalpha * 0)
 * newcolor = (srccolor * (1 - dstalpha)) + (dstcolor * 0)
 * =>
 * newalpha = srcalpha - srcalpha * dstalpha
 * newcolor = srccolor - srccolor * dstalpha
 *
 * if srcalpha == 0 or
 *    dstalpha == 1:  newalpha = 0
 *                    newcolor = 0
 * if dstalpha == 0:  newalpha = srcalpha   // can just copy
 *                    newcolor = srccolor
 * if srcalpha == 1:  newalpha = 1 - dstalpha
 *                    newcolor = srccolor - srccolor * dstalpha  or  srccolor * (1 - dstalpha)
 * else:              newalpha = srcalpha - srcalpha * dstalpha
 *                    newcolor = srccolor - srccolor * dstalpha
 *
 * Destination Out:
 *
 * fraction_a = 0, fraction_b = 1 - srcalpha
 *
 * newalpha = (srcalpha * 0) + dstalpha * (1 - srcalpha))
 * newcolor = (srccolor * 0) + dstcolor * (1 - srcalpha))
 * =>
 * newalpha = dstalpha - dstalpha * srcalpha
 * newcolor = dstcolor - dstcolor * srcalpha
 *
 * if srcalpha == 0:  newalpha = dstalpha   // no-op when result image is destination image
 *                    newcolor = dstcolor
 * if dstalpha == 0 or
 *    srcalpha == 1:  newalpha = 0
 *                    newcolor = 0
 * if dstalpha == 1:  newalpha = 1 - srcalpha
 *                    newcolor = dstcolor - dstcolor * srcalpha  or  dstcolor * (1 - srcalpha)
 * else:              newalpha = dstalpha - dstalpha * srcalpha
 *                    newcolor = dstcolor - dstcolor * srcalpha
 *
 * Source Atop:
 *
 * fraction_a = dstalpha, fraction_b = 1 - srcalpha
 *
 * newalpha = (srcalpha * dstalpha) + (dstalpha * (1 - srcalpha))
 * newcolor = (srccolor * dstalpha) + (dstcolor * (1 - srcalpha))
 * =>
 * newalpha = srcalpha * dstalpha + dstalpha - dstalpha * srcalpha
 * newcolor = srccolor * dstalpha + dstcolor - dstcolor * srcalpha
 * =>
 * newalpha = dstalpha
 *
 * if srcalpha == 0:  newalpha = dstalpha   // no-op when result image is destination imag
 *                    newcolor = dstcolor
 * if dstalpha == 0:  newalpha = 0
 *                    newcolor = 0
 * if srcalpha == 1:  newalpha = dstalpha
 *                    newcolor = srccolor * dstalpha
 * if dstalpha == 1:  newalpha = 1
 *                    newcolor = srccolor + dstcolor - dstcolor * srcalpha  // quick blend case
 * else:              newalpha = dstalpha
 *                    newcolor = srccolor * dstalpha + dstcolor - dstcolor * srcalpha
 *
 * Destination Atop:
 *
 * fraction_a = 1 - dstalpha, fraction_b = srcalpha
 *
 * newalpha = (srcalpha * (1 - dstalpha)) + (dstalpha * srcalpha)
 * newcolor = (srccolor * (1 - dstalpha)) + (dstcolor * srcalpha)
 * =>
 * newalpha = srcalpha - srcalpha * dstalpha + dstalpha * srcalpha
 * newcolor = srccolor - srccolor * dstalpha + dstcolor * srcalpha
 * =>
 * newalpha = srcalpha
 *
 * if srcalpha == 0:  newalpha = 0
 *                    newcolor = 0
 * if dstalpha == 0:  newalpha = srcalpha
 *                    newcolor = srccolor
 * if srcalpha == 1:  newalpha = 1
 *                    newcolor = srccolor + dstcolor - srccolor * dstalpha  // quick blend case
 * if dstalpha == 1:  newalpha = srcalpha
 *                    newcolor = dstcolor * srcalpha
 * else:              newalpha = srcalpha
 *                    newcolor = srccolor - srccolor * dstalpha + dstcolor * srcalpha
 *
 * XOR:
 *
 * fraction_a = 1 - dstalpha, fraction_b = 1 - srcalpha
 *
 * newalpha = (srcalpha * (1 - dstalpha)) + (dstalpha * (1 - srcalpha))
 * newcolor = (srccolor * (1 - dstalpha)) + (dstcolor * (1 - srcalpha))
 * =>
 * newalpha = srcalpha - srcalpha * dstalpha + dstalpha - dstalpha * srcalpha
 * newcolor = srccolor - srccolor * dstalpha + dstcolor - dstcolor * srcalpha
 * =>
 * newalpha = srcalpha + dstalpha - (2 * srcalpha * dstalpha)
 *
 * if srcalpha == 0: newalpha = dstalpha
 *                   newcolor = dstcolor
 * if srcalpha == 1: newalpha = 1 - dstalpha
 *                   newcolor = srccolor * (1 - dstalpha)
 * if dstalpha == 0: newalpha = srcalpha
 *                   newcolor = srccolor
 * if dstalpha == 1: newalpha = 1 - srcalpha
 *                   newcolor = dstcolor * (1 - srcalpha)
 * else:             newalpha = (srcalpha * (1 - dstalpha)) + (dstalpha * (1 - srcalpha))
 *                   newcolor = (srccolor * (1 - dstalpha)) + (dstcolor * (1 - srcalpha))
 */

/* ----------------------------------------------------------------------- */

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "base/utils.h"

#include "utils/maths.h"

#include "framebuf/pixelfmt.h"

#include "framebuf/composite.h"

/* ----------------------------------------------------------------------- */

/* Shorthands for pixel macros */

/* These constants are correct for pixelfmt_rgba8888 but the code treats colour
 * components identically so R,G,B could be in the wrong order and I wouldn't
 * know. */

#define RED_SHIFT     PIXELFMT_Rxxx8888_SHIFT
#define GREEN_SHIFT   PIXELFMT_xGxx8888_SHIFT
#define BLUE_SHIFT    PIXELFMT_xxBx8888_SHIFT
#define ALPHA_SHIFT   PIXELFMT_xxxA8888_SHIFT

#define ALPHA_MASK    PIXELFMT_xxxA8888_MASK

#define RED(px)       PIXELFMT_Rxxx8888(px)
#define GREEN(px)     PIXELFMT_xGxx8888(px)
#define BLUE(px)      PIXELFMT_xxBx8888(px)
#define ALPHA(px)     PIXELFMT_xxxA8888(px)

#define FUSE(r,g,b,a) PIXELFMT_MAKE_RGBA8888(r,g,b,a)

/* ----------------------------------------------------------------------- */

/* The type of a compositing routine. */
typedef void (compo_t)(const pixelfmt_xxxa8888_t *src,
                       pixelfmt_xxxa8888_t       *dst,
                       unsigned int               width);

/* The type of a pixel component. */
typedef unsigned int pixelcmp_t;

/* ----------------------------------------------------------------------- */

/* Weighted mean of src's and dst's colours, src weighted by ws and dst by
 * wd, each an alpha product scaled by 255 * 255, with ws + wd <= 255 * 255.
 * The result's alpha is (ws + wd) / 255.
 *
 * Dividing by the unrounded weight sum, not by the rounded 8-bit alpha,
 * keeps each channel within 0.5 of exact, and so never above 255. The
 * single division per pixel is memoised in memo[] (both zero to start) so
 * that runs of matching alphas skip it. */
static INLINE pixelfmt_xxxa8888_t blend_weighted(pixelfmt_xxxa8888_t src,
                                                 pixelfmt_xxxa8888_t dst,
                                                 pixelcmp_t          ws,
                                                 pixelcmp_t          wd,
                                                 unsigned int        memo[2])
{
  pixelcmp_t den;
  pixelcmp_t newalpha;

  den      = ws + wd;
  newalpha = div255(den);
  if (newalpha == 0)
    return 0;

  if (den != memo[0])
  {
    memo[0] = den;
    memo[1] = DIV_RECIP(den);
  }

#define BLEND(shift) \
  ((pixelfmt_xxxa8888_t) div_recip(((src >> shift) & 0xFF) * ws + \
                                   ((dst >> shift) & 0xFF) * wd + den / 2, \
                                   den, memo[1]) << shift)
  return BLEND(RED_SHIFT) | BLEND(GREEN_SHIFT) | BLEND(BLUE_SHIFT) |
         ((pixelfmt_xxxa8888_t) newalpha << ALPHA_SHIFT);
#undef BLEND
}

/* ----------------------------------------------------------------------- */

/**
 * 'Clear'. Both the colour and the alpha of the destination are cleared.
 */
static void comp_xxxa8888_clear(const pixelfmt_xxxa8888_t *src,
                                pixelfmt_xxxa8888_t       *dst,
                                unsigned int               width)
{
  memset(dst, 0, width * sizeof(*src));
}

/* ----------------------------------------------------------------------- */

/**
 * 'Source'. The source is copied to the destination.
 */
static void comp_xxxa8888_src(const pixelfmt_xxxa8888_t *src,
                              pixelfmt_xxxa8888_t       *dst,
                              unsigned int               width)
{
  memcpy(dst, src, width * sizeof(*src));
}

/* ----------------------------------------------------------------------- */

#ifdef SPEEDUPS
/* 'Source over/Destination over' opaque blend case which can be used when
 * dst.alpha is solid (255). Used for 'Source atop/Destination atop' too. */
static INLINE pixelfmt_xxxa8888_t blend_xxxa8888_src_over_opaque(pixelfmt_xxxa8888_t src,
                                                                 pixelfmt_xxxa8888_t dst)
{
  pixelcmp_t srcalpha;
  pixelcmp_t csrcalpha;
  pixelcmp_t srcR, srcG, srcB;
  pixelcmp_t dstR, dstG, dstB;
  pixelcmp_t newR, newG, newB;

  /* Ensure that the outer code is correctly handling the non-opaque cases. */
  assert(ALPHA(dst) == 255);

  /* alpha part comes first */

  srcalpha  = ALPHA(src);
  csrcalpha = 255 - srcalpha; /* complement of source alpha */

  /* colour part comes second */

  srcR = RED(src);
  srcG = GREEN(src);
  srcB = BLUE(src);

  dstR = RED(dst);
  dstG = GREEN(dst);
  dstB = BLUE(dst);

#define BLEND(src, dst) div255(src * srcalpha + dst * csrcalpha)
  newR = BLEND(srcR, dstR);
  newG = BLEND(srcG, dstG);
  newB = BLEND(srcB, dstB);
#undef BLEND

  return FUSE(newR, newG, newB, 255);
}
#endif

/* 'Source over/Destination over' general blend case. */
static INLINE pixelfmt_xxxa8888_t blend_xxxa8888_src_over_general(pixelfmt_xxxa8888_t src,
                                                                  pixelfmt_xxxa8888_t dst,
                                                                  unsigned int        memo[2])
{
  pixelcmp_t srcalpha;
  pixelcmp_t dstalpha;

  srcalpha = ALPHA(src);
  dstalpha = ALPHA(dst);

  return blend_weighted(src, dst,
                        srcalpha * 255,
                        dstalpha * (255 - srcalpha),
                        memo);
}

/**
 * 'Source over'. The source is composed with the destination, and the
 * result replaces the destination.
 */
static void comp_xxxa8888_src_over(const pixelfmt_xxxa8888_t *src,
                                   pixelfmt_xxxa8888_t       *dst,
                                   unsigned int               width)
{
  unsigned int memo[2];

  memo[0] = 0;
  memo[1] = 0; /* never read before set; quietens -Wmaybe-uninitialized */

  while (width--)
  {
#ifdef SPEEDUPS
    pixelcmp_t srcalpha = ALPHA(*src);

    if (srcalpha != 0) /* srcalpha == 0 => preserve */
    {
      pixelcmp_t dstalpha = ALPHA(*dst);

      if (srcalpha == 255 || dstalpha == 0) /* overwrite */
        *dst = *src;
      else if (dstalpha == 255) /* overwrite */
        *dst = blend_xxxa8888_src_over_opaque(*src, *dst);
      else
#endif
        *dst = blend_xxxa8888_src_over_general(*src, *dst, memo);
#ifdef SPEEDUPS
    }
#endif

    src++;
    dst++;
  }
}

/* ----------------------------------------------------------------------- */

/* Initially it looks like destination over can be implemented by just taking
 * source over and swapping the arguments around. But for this interface the
 * result B is overwritten, so we'd end up trying to overwrite the input
 * A if we only swapped the args.
 */

/**
 * 'Destination over'. The destination is composed with the source, and the
 * result replaces the destination.
 */
static void comp_xxxa8888_dst_over(const pixelfmt_xxxa8888_t *src,
                                   pixelfmt_xxxa8888_t       *dst,
                                   unsigned int               width)
{
  unsigned int memo[2];

  memo[0] = 0;
  memo[1] = 0; /* never read before set; quietens -Wmaybe-uninitialized */

  while (width--)
  {
#ifdef SPEEDUPS
    pixelcmp_t srcalpha = ALPHA(*src);
    pixelcmp_t dstalpha = ALPHA(*dst);

    if (srcalpha == 0 || dstalpha == 255) /* preserve */
      ;
    else if (dstalpha == 0) /* overwrite */
      *dst = *src;
    else if (srcalpha == 255) /* overwrite */
      *dst = blend_xxxa8888_src_over_opaque(*dst, *src); /* reversed args */
    else
#endif
      *dst = blend_xxxa8888_src_over_general(*dst, *src, memo); /* reversed args */

    src++;
    dst++;
  }
}

/* ----------------------------------------------------------------------- */

/* 'Source in/Destination in' general blend case. */
static INLINE pixelfmt_xxxa8888_t blend_xxxa8888_src_in_general(pixelfmt_xxxa8888_t src,
                                                                pixelfmt_xxxa8888_t dst)
{
  pixelcmp_t srcalpha;
  pixelcmp_t dstalpha;
  pixelcmp_t newalpha;

  /* alpha part comes first */
  srcalpha = ALPHA(src);
  dstalpha = ALPHA(dst);

  newalpha = div255(srcalpha * dstalpha);
  if (newalpha == 0)
    return 0;

  /* colour part comes second */
  return (src & ~ALPHA_MASK) | (newalpha << ALPHA_SHIFT);
}

/**
 * 'Source in'. The part of the source lying inside of the destination
 * replaces the destination. Everything else is cleared.
 */
static void comp_xxxa8888_src_in(const pixelfmt_xxxa8888_t *src,
                                 pixelfmt_xxxa8888_t       *dst,
                                 unsigned int               width)
{
  while (width--)
  {
#ifdef SPEEDUPS
    pixelcmp_t srcalpha = ALPHA(*src);
    pixelcmp_t dstalpha = ALPHA(*dst);

    if (srcalpha == 0 || dstalpha == 0) /* overwrite */
      *dst = 0;
    else if (dstalpha == 255) /* overwrite */
      *dst = *src;
    else if (srcalpha == 255) /* overwrite */
      *dst = (*src & ~ALPHA_MASK) | (dstalpha << ALPHA_SHIFT);
    else
#endif
      *dst = blend_xxxa8888_src_in_general(*src, *dst);

    src++;
    dst++;
  }
}

/* ----------------------------------------------------------------------- */

/**
 * 'Destination in'. The part of the destination lying inside of the source
 * replaces the destination.
 */
static void comp_xxxa8888_dst_in(const pixelfmt_xxxa8888_t *src,
                                 pixelfmt_xxxa8888_t       *dst,
                                 unsigned int               width)
{
  while (width--)
  {
#ifdef SPEEDUPS
    pixelcmp_t srcalpha = ALPHA(*src);
    pixelcmp_t dstalpha = ALPHA(*dst);

    if (srcalpha == 255) /* preserve */
      ;
    else if (srcalpha == 0 || dstalpha == 0) /* overwrite */
      *dst = 0;
    else if (dstalpha == 255) /* overwrite */
      *dst = (*dst & ~ALPHA_MASK) | (srcalpha << ALPHA_SHIFT);
    else
#endif
      *dst = blend_xxxa8888_src_in_general(*dst, *src); /* reversed args */

    src++;
    dst++;
  }
}

/* ----------------------------------------------------------------------- */

/* 'Source out/Destination out' general blend case. */
static INLINE pixelfmt_xxxa8888_t blend_xxxa8888_src_out_general(pixelfmt_xxxa8888_t src,
                                                                 pixelfmt_xxxa8888_t dst)
{
  pixelcmp_t srcalpha;
  pixelcmp_t dstalpha;
  pixelcmp_t newalpha;

  /* alpha part comes first */
  srcalpha = ALPHA(src);
  dstalpha = ALPHA(dst);

  newalpha = div255(srcalpha * (255 - dstalpha));
  if (newalpha == 0)
    return 0;

  /* colour part comes second: only src contributes, so its colour is kept */
  return (src & ~ALPHA_MASK) | (newalpha << ALPHA_SHIFT);
}

/**
 * 'Source out'. The part of the source lying outside of the destination
 * replaces the destination. The part of the source inside the destination
 * gets discarded.
 */
static void comp_xxxa8888_src_out(const pixelfmt_xxxa8888_t *src,
                                  pixelfmt_xxxa8888_t       *dst,
                                  unsigned int               width)
{
  while (width--)
  {
#ifdef SPEEDUPS
    pixelcmp_t srcalpha = ALPHA(*src);
    pixelcmp_t dstalpha = ALPHA(*dst);

    if (srcalpha == 0 || dstalpha == 255) /* overwrite */
      *dst = 0;
    else if (dstalpha == 0) /* overwrite */
      *dst = *src;
    else if (srcalpha == 255) /* overwrite */
      *dst = (*src & ~ALPHA_MASK) | ((255 - dstalpha) << ALPHA_SHIFT);
    else
#endif
      *dst = blend_xxxa8888_src_out_general(*src, *dst);

    src++;
    dst++;
  }
}

/* ----------------------------------------------------------------------- */

/**
 * 'Destination out'. The part of the destination lying outside of the source
 * replaces the destination.
 */
static void comp_xxxa8888_dst_out(const pixelfmt_xxxa8888_t *src,
                                  pixelfmt_xxxa8888_t       *dst,
                                  unsigned int               width)
{
  while (width--)
  {
#ifdef SPEEDUPS
    pixelcmp_t srcalpha = ALPHA(*src);
    pixelcmp_t dstalpha = ALPHA(*dst);

    if (srcalpha == 0) /* preserve */
      ;
    else if (srcalpha == 255 || dstalpha == 0) /* overwrite */
      *dst = 0;
    else if (dstalpha == 255) /* overwrite */
      *dst = (*dst & ~ALPHA_MASK) | ((255 - srcalpha) << ALPHA_SHIFT);
    else
#endif
      *dst = blend_xxxa8888_src_out_general(*dst, *src); /* reversed args */

    src++;
    dst++;
  }
}

/* ----------------------------------------------------------------------- */

/* 'Source atop/Destination atop' general blend case. */
static INLINE pixelfmt_xxxa8888_t blend_xxxa8888_src_atop_general(pixelfmt_xxxa8888_t src,
                                                                  pixelfmt_xxxa8888_t dst)
{
  pixelcmp_t srcalpha;
  pixelcmp_t csrcalpha;
  pixelcmp_t newalpha;
  pixelcmp_t newR, newG, newB;

  /* alpha part comes first */

  srcalpha  = ALPHA(src);
  csrcalpha = 255 - srcalpha;

  newalpha = ALPHA(dst);
  if (newalpha == 0)
    return 0; /* alpha was zero */

  /* colour part comes second: dst's alpha weights both terms, so it
   * cancels, leaving a plain lerp by srcalpha */

#define BLEND(src, dst) div255((src) * srcalpha + (dst) * csrcalpha)
  newR = BLEND(RED(src), RED(dst));
  newG = BLEND(GREEN(src), GREEN(dst));
  newB = BLEND(BLUE(src), BLUE(dst));
#undef BLEND

  return FUSE(newR, newG, newB, newalpha);
}

/**
 * 'Source atop'. The part of the source lying inside of the destination is
 * composed with the destination. The part of the source lying outside of the
 * destination is discarded.
 */
static void comp_xxxa8888_src_atop(const pixelfmt_xxxa8888_t *src,
                                   pixelfmt_xxxa8888_t       *dst,
                                   unsigned int               width)
{
  while (width--)
  {
#ifdef SPEEDUPS
    pixelcmp_t srcalpha = ALPHA(*src);
    pixelcmp_t dstalpha = ALPHA(*dst);

    if (srcalpha == 0) /* preserve */
      ;
    else if (dstalpha == 0) /* overwrite */
      *dst = 0;
    else if (srcalpha == 255) /* overwrite */
      *dst = (*src & ~ALPHA_MASK) | (dstalpha << ALPHA_SHIFT);
    else if (dstalpha == 255) /* overwrite */
      *dst = blend_xxxa8888_src_over_opaque(*src, *dst);
    else
#endif
      *dst = blend_xxxa8888_src_atop_general(*src, *dst);

    src++;
    dst++;
  }
}

/* ----------------------------------------------------------------------- */

/**
 * 'Destination atop'. The part of the destination lying inside of the source
 * is composed with the source and replaces the destination.
 */
static void comp_xxxa8888_dst_atop(const pixelfmt_xxxa8888_t *src,
                                   pixelfmt_xxxa8888_t       *dst,
                                   unsigned int               width)
{
  while (width--)
  {
#ifdef SPEEDUPS
    pixelcmp_t srcalpha = ALPHA(*src);
    pixelcmp_t dstalpha = ALPHA(*dst);

    if (srcalpha == 0) /* overwrite */
      *dst = 0;
    else if (dstalpha == 0) /* overwrite */
      *dst = *src;
    else if (srcalpha == 255) /* overwrite */
      *dst = blend_xxxa8888_src_over_opaque(*dst, *src); /* reversed args */
    else if (dstalpha == 255) /* overwrite */
      *dst = (*dst & ~ALPHA_MASK) | (srcalpha << ALPHA_SHIFT);
    else
#endif
      *dst = blend_xxxa8888_src_atop_general(*dst, *src); /* reversed args */

    src++;
    dst++;
  }
}

/* ----------------------------------------------------------------------- */

/* 'XOR' general blend case. */
static INLINE pixelfmt_xxxa8888_t blend_xxxa8888_xor_general(pixelfmt_xxxa8888_t src,
                                                             pixelfmt_xxxa8888_t dst,
                                                             unsigned int        memo[2])
{
  pixelcmp_t srcalpha;
  pixelcmp_t dstalpha;

  srcalpha = ALPHA(src);
  dstalpha = ALPHA(dst);

  return blend_weighted(src, dst,
                        srcalpha * (255 - dstalpha),
                        dstalpha * (255 - srcalpha),
                        memo);
}

/**
 * 'XOR'. The part of the destination which lies outside of the destination
 * is combined with the part of the destination that lies outside of the
 * source.
 */
static void comp_xxxa8888_xor(const pixelfmt_xxxa8888_t *src,
                              pixelfmt_xxxa8888_t       *dst,
                              unsigned int               width)
{
  unsigned int memo[2];

  memo[0] = 0;
  memo[1] = 0; /* never read before set; quietens -Wmaybe-uninitialized */

  while (width--)
  {
#ifdef SPEEDUPS
    pixelcmp_t srcalpha = ALPHA(*src);
    pixelcmp_t dstalpha = ALPHA(*dst);

    if (srcalpha == 0) /* preserve */
      ;
    else if (dstalpha == 0) /* overwrite */
      *dst = *src;
    else if (srcalpha == 255) /* overwrite */
      *dst = (*src & ~ALPHA_MASK) | ((255 - dstalpha) << ALPHA_SHIFT);
    else if (dstalpha == 255) /* overwrite */
      *dst = (*dst & ~ALPHA_MASK) | ((255 - srcalpha) << ALPHA_SHIFT);
    else
#endif
      *dst = blend_xxxa8888_xor_general(*src, *dst, memo);

    src++;
    dst++;
  }
}

/* ----------------------------------------------------------------------- */

static void composite_xxxa8888(composite_rule_t rule,
                               const bitmap_t  *src,
                               bitmap_t        *dst)
{
  static compo_t *compos[composite_RULE__LIMIT] =
  {
    comp_xxxa8888_clear,    /* composite_RULE_CLEAR */
    comp_xxxa8888_src,      /* composite_RULE_SRC */
    NULL,                   /* composite_RULE_DST */
    comp_xxxa8888_src_over, /* composite_RULE_SRC_OVER */
    comp_xxxa8888_dst_over, /* composite_RULE_DST_OVER */
    comp_xxxa8888_src_in,   /* composite_RULE_SRC_IN */
    comp_xxxa8888_dst_in,   /* composite_RULE_DST_IN */
    comp_xxxa8888_src_out,  /* composite_RULE_SRC_OUT */
    comp_xxxa8888_dst_out,  /* composite_RULE_DST_OUT */
    comp_xxxa8888_src_atop, /* composite_RULE_SRC_ATOP */
    comp_xxxa8888_dst_atop, /* composite_RULE_DST_ATOP */
    comp_xxxa8888_xor       /* composite_RULE_XOR */
  };

  compo_t                   *compo;
  const pixelfmt_xxxa8888_t *srcscan; /* source scanline */
  pixelfmt_xxxa8888_t       *dstscan; /* destination scanline */
  int                        width, height;
  int                        rowbytes;

  /* The 'destination' rule is a no-op so just bypass it. */
  if (rule == composite_RULE_DST)
    return;

  compo = compos[rule];

  srcscan  = src->base;
  dstscan  = dst->base;
  width    = src->size.w;
  height   = src->size.h;
  rowbytes = src->rowbytes / sizeof(pixelfmt_xxxa8888_t);

  while (height--)
  {
    compo(srcscan, dstscan, width);
    srcscan += rowbytes;
    dstscan += rowbytes;
  }
}

result_t composite(composite_rule_t rule,
                   const bitmap_t  *src,
                   bitmap_t        *dst)
{
  assert(rule <= composite_RULE__LIMIT);
  assert(src != NULL);
  assert(dst != NULL);

  if (src == NULL || dst == NULL)
    return result_NULL_ARG;

  if (src->size.w  != dst->size.w  ||
      src->size.h != dst->size.h ||
      src->format != dst->format)
    return result_BAD_ARG;

  if (pixelfmt_is_rle(src->format) || pixelfmt_is_rle(dst->format))
    return result_NOT_SUPPORTED;

  switch (src->format)
  {
  case pixelfmt_rgba8888:
  case pixelfmt_bgra8888:
    composite_xxxa8888(rule, src, dst);
    break;

  case pixelfmt_abgr8888:
  case pixelfmt_argb8888:
  case pixelfmt_rgb565:   /* no alpha channel */
  case pixelfmt_rgbx5551: /* no alpha channel */
  default:
    return result_NOT_IMPLEMENTED;
  }

  return result_OK;
}

/* ----------------------------------------------------------------------- */

pixelfmt_xxxa8888_t composite_over_pixel(pixelfmt_xxxa8888_t dst,
                                         pixelfmt_xxxa8888_t src,
                                         unsigned int        opacity)
{
  unsigned int        sa, da, oa;
  pixelfmt_xxxa8888_t out;
  unsigned int        i;
  unsigned int        c;

  sa = div255((src >> 24) * opacity);
  da = dst >> 24;
  if (sa == 0)
    return dst;
  if (sa == 255 || da == 0)
    return (src & 0xFFFFFFu) | (sa << 24);

  da  = div255(da * (255 - sa)); /* what shows of dst */
  oa  = sa + da;
  out = oa << 24;
  for (i = 0; i < 24; i += 8)
  {
    c = ((src >> i) & 0xFF) * sa + ((dst >> i) & 0xFF) * da;
    /* a divide per channel only where dst is partly transparent: opaque
     * below, the common case, takes the shift */
    c = oa == 255 ? div255(c) : (c + oa / 2) / oa;
    out |= c << i;
  }

  return out;
}
