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

#ifdef __cplusplus
}
#endif

#endif /* COMPOSITE_H */
