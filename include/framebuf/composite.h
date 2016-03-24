/* composite.h */

#ifndef COMPOSITE_H
#define COMPOSITE_H

#include "base/result.h"

#include "framebuf/bitmap.h"
#include "framebuf/pixelfmt.h"

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
 * Composites 'src' over 'dst' (or other rule).
 *
 * \param[in] rule Compositing rule to use.
 * \param[in] src  Source bitmap.
 * \param[in] dst  Destination bitmap.
 *
 * \return Result.
 *
 * The given bitmaps must have alpha channels.
 */
result_t composite(composite_rule_t rule,
                   const bitmap_t  *src,
                   bitmap_t        *dst);

#endif /* COMPOSITE_H */
