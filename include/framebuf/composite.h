/* framebuf/composite.h -- Porter-Duff image compositing */

#ifndef COMPOSITE_H
#define COMPOSITE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"

#include "framebuf/bitmap.h"
#include "framebuf/pixelfmt.h"

/** Compositing rules. */
typedef enum composite_rule
{
  composite_RULE_CLEAR,
  composite_RULE_SRC,
  composite_RULE_DST,
  composite_RULE_SRC_OVER,
  composite_RULE_DST_OVER,
  composite_RULE_SRC_IN,
  composite_RULE_DST_IN,
  composite_RULE_SRC_OUT,
  composite_RULE_DST_OUT,
  composite_RULE_SRC_ATOP,
  composite_RULE_DST_ATOP,
  composite_RULE_XOR,
  composite_RULE__LIMIT
}
composite_rule_t;

/**
 * Composites bitmap `src` with `dst` using the specified compositing `rule`.
 *
 * The destination bitmap is modified in place.
 *
 * Note: The given bitmaps must have alpha channels.
 *
 * \param[in]     rule Compositing rule to use.
 * \param[in]     src  First source bitmap.
 * \param[in,out] dst  Second source and destination bitmap.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t composite(composite_rule_t rule,
                   const bitmap_t  *src,
                   bitmap_t        *dst);

/**
 * Composites one straight-alpha pixel over another (source over), first
 * scaling the source's alpha by `opacity`. Works for any 32 bpp format with
 * alpha in the top byte.
 *
 * Divides only where `dst` is partly transparent.
 *
 * \param[in] dst     Pixel underneath.
 * \param[in] src     Pixel on top.
 * \param[in] opacity Opacity of `src`, 0 (none) to 255 (as is).
 * \return The composited pixel.
 */
pixelfmt_xxxa8888_t composite_over_pixel(pixelfmt_xxxa8888_t dst,
                                         pixelfmt_xxxa8888_t src,
                                         unsigned int        opacity);

#ifdef __cplusplus
}
#endif

#endif /* COMPOSITE_H */
