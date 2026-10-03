/* framebuf/effects/clear.c -- fill the colour channels */

#include <stdint.h>

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/colour.h"

#include "framebuf/effects.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

result_t effects_clear_apply(const bitmap_t *src,
                             bitmap_t       *dst,
                             colour_t        colour)
{
  result_t         rc;
  effects_layout_t layout;
  unsigned int     r, g, b;
  uint32_t         fill;
  unsigned int     amask;
  int              x, y;

  rc = effects__check(src, dst, &layout);
  if (rc != result_OK)
    return rc;

  colour_get_rgb(&colour, &r, &g, &b);

  fill = (r << layout.r) | (g << layout.g) | (b << layout.b);

  amask = EFFECTS_ALPHA_MASK(&layout);

  for (y = 0; y < src->size.h; y++)
  {
    const uint32_t *sp;
    uint32_t       *dp;

    sp = EFFECTS_ROW(src, y);
    dp = EFFECTS_ROW(dst, y);

    for (x = 0; x < src->size.w; x++)
      dp[x] = fill | (sp[x] & amask);
  }

  return result_OK;
}
