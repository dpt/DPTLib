/* framebuf/effects/grey.c -- greyscale effect */

#include <stdint.h>

#include "base/result.h"
#include "framebuf/bitmap.h"

#include "framebuf/effects.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

result_t effects_grey_apply(const bitmap_t *src, bitmap_t *dst)
{
  /* libjpeg's weights */
  const int red_weight   = 19595; /* 0.29900 * 65536 (rounded down) */
  const int green_weight = 38470; /* 0.58700 * 65536 (rounded up)   */
  const int blue_weight  =  7471; /* 0.11400 * 65536 (rounded down) */

  result_t         rc;
  effects_layout_t layout;
  unsigned int     amask;
  int              x, y;

  rc = effects__check(src, dst, &layout);
  if (rc != result_OK)
    return rc;

  amask = EFFECTS_ALPHA_MASK(&layout);

  for (y = 0; y < src->size.h; y++)
  {
    const uint32_t *sp;
    uint32_t       *dp;

    sp = EFFECTS_ROW(src, y);
    dp = EFFECTS_ROW(dst, y);

    for (x = 0; x < src->size.w; x++)
    {
      uint32_t px;
      int      r, g, b;
      uint32_t gr;

      px = sp[x];

      r = (px >> layout.r) & 0xFF;
      g = (px >> layout.g) & 0xFF;
      b = (px >> layout.b) & 0xFF;

      if (r == g && g == b)
        gr = (uint32_t) r;
      else
        gr = (uint32_t) ((r * red_weight + g * green_weight +
                          b * blue_weight) >> 16);

      dp[x] = (gr << layout.r) | (gr << layout.g) | (gr << layout.b) |
              (px & amask);
    }
  }

  return result_OK;
}
