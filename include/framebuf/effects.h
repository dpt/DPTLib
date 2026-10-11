/* framebuf/effects.h -- bitmap effects */

/**
 * \file effects.h
 *
 * Whole-bitmap effects: blur, sharpen, blend, grey, clear, expand, equalise
 * and tone mapping.
 *
 * Every effect works on the 32bpp formats (`rgbx8888`, `bgrx8888`,
 * `xbgr8888`, `xrgb8888`, `rgba8888`, `bgra8888`, `abgr8888` and
 * `argb8888`). The caller supplies a destination of the same size and format
 * as the source. Source and destination may be the same bitmap unless stated
 * otherwise. Channel order is taken from the pixel format. Except for \ref
 * effects_blend and \ref effects_tonemap_apply, the fourth (alpha or
 * padding) byte of each pixel is copied unchanged.
 *
 * Each function returns \ref result_BAD_ARG if the bitmaps differ in size or
 * format or if the format is unsupported.
 */

#ifndef FRAMEBUF_EFFECTS_H
#define FRAMEBUF_EFFECTS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"
#include "framebuf/tonemap.h"

/* ----------------------------------------------------------------------- */

/** Kernel shape used by \ref effects_blur_apply. */
typedef enum effects_blur_method
{
  effects_blur_BOX,
  effects_blur_GAUSSIAN,
}
effects_blur_method_t;

/** Smallest blur amount: a 5-point kernel. */
#define effects_blur_MIN_AMOUNT (2)

/** Largest blur amount: a 95-point kernel. */
#define effects_blur_MAX_AMOUNT (48)

/**
 * Blur a bitmap with a separable kernel.
 *
 * Edge pixels are replicated outwards.
 *
 * \param[in]  src    Source bitmap.
 * \param[out] dst    Destination bitmap.
 * \param[in]  method Kernel shape.
 * \param[in]  amount Kernel half-width, clamped to \ref
 *                    effects_blur_MIN_AMOUNT to \ref
 *                    effects_blur_MAX_AMOUNT.
 * \return \ref result_OK on success, \ref result_BAD_ARG for unsuitable
 *         bitmaps, \ref result_OOM on allocation failure.
 */
result_t effects_blur_apply(const bitmap_t       *src,
                            bitmap_t             *dst,
                            effects_blur_method_t method,
                            int                   amount);

/**
 * Sharpen a bitmap.
 *
 * \param[in]  src Source bitmap.
 * \param[out] dst Destination bitmap.
 * \return \ref result_OK on success, \ref result_BAD_ARG for unsuitable
 *         bitmaps, \ref result_OOM on allocation failure.
 */
result_t effects_sharpen_apply(const bitmap_t *src, bitmap_t *dst);

/**
 * Blend two bitmaps, every byte of each pixel included.
 *
 * Each destination byte is `(a * (256 - alpha) + b * alpha) >> 8`. An
 * `alpha` of 0 yields `a` and 256 yields `b`.
 *
 * \param[in]  a     First bitmap.
 * \param[in]  b     Second bitmap.
 * \param[out] dst   Destination bitmap.
 * \param[in]  alpha Weight of `b`, clamped to 0 to 256.
 * \return \ref result_OK on success, \ref result_BAD_ARG for unsuitable
 *         bitmaps.
 */
result_t effects_blend(const bitmap_t *a,
                       const bitmap_t *b,
                       bitmap_t       *dst,
                       int             alpha);

/**
 * Fill the colour channels with a single colour, keeping each pixel's alpha.
 *
 * \param[in]  src    Source bitmap, supplies the alpha bytes.
 * \param[out] dst    Destination bitmap.
 * \param[in]  colour Colour to fill with; its alpha is ignored.
 * \return \ref result_OK on success, \ref result_BAD_ARG for unsuitable
 *         bitmaps.
 */
result_t effects_clear_apply(const bitmap_t *src,
                             bitmap_t       *dst,
                             colour_t        colour);

/**
 * Equalise the histogram of each colour channel.
 *
 * \param[in]  src Source bitmap.
 * \param[out] dst Destination bitmap.
 * \return \ref result_OK on success, \ref result_BAD_ARG for unsuitable
 *         bitmaps.
 */
result_t effects_equalise_apply(const bitmap_t *src, bitmap_t *dst);

/**
 * Stretch the used range of the colour channels to fill 0 to 255.
 *
 * The range is the span, over all three channels, of the levels used by more
 * than `threshold` pixels. Levels below it become 0 and levels above it 255.
 * If the range is already full or empty the bitmap is copied unchanged.
 *
 * \param[in]  src       Source bitmap.
 * \param[out] dst       Destination bitmap.
 * \param[in]  threshold Pixel count a level must exceed to be counted.
 * \return \ref result_OK on success, \ref result_BAD_ARG for unsuitable
 *         bitmaps.
 */
result_t effects_expand_apply(const bitmap_t *src,
                              bitmap_t       *dst,
                              unsigned int    threshold);

/**
 * Convert a bitmap to grey, keeping each pixel's alpha.
 *
 * \param[in]  src Source bitmap.
 * \param[out] dst Destination bitmap.
 * \return \ref result_OK on success, \ref result_BAD_ARG for unsuitable
 *         bitmaps.
 */
result_t effects_grey_apply(const bitmap_t *src, bitmap_t *dst);

/**
 * Apply a tone map to a bitmap.
 *
 * All four bytes of each pixel are mapped; the alpha table is linear unless
 * the tone map has an alpha channel.
 *
 * \param[in]  src Source bitmap.
 * \param[out] dst Destination bitmap.
 * \param[in]  map Tone map.
 * \return \ref result_OK on success, \ref result_BAD_ARG for unsuitable
 *         bitmaps.
 */
result_t effects_tonemap_apply(const bitmap_t *src,
                               bitmap_t       *dst,
                               tonemap_t      *map);

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* FRAMEBUF_EFFECTS_H */
