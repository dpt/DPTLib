/* framebuf/effects/blend.c -- alpha blending between two bitmaps */

#include <stdint.h>

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"

#include "framebuf/effects.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

result_t effects_blend(const bitmap_t *a,
                       const bitmap_t *b,
                       bitmap_t       *dst,
                       int             alpha)
{
  result_t         rc;
  effects_layout_t layout;
  int              calpha; /* complement of alpha */
  int              x, y;

  rc = effects__check(a, dst, &layout);
  if (rc != result_OK)
    return rc;

  rc = effects__check(a, b, &layout);
  if (rc != result_OK)
    return rc;

  alpha  = CLAMP(alpha, 0, 256);
  calpha = 256 - alpha;

  for (y = 0; y < a->size.h; y++)
  {
    const uint32_t *ap;
    const uint32_t *bp;
    uint32_t       *dp;

    ap = EFFECTS_ROW(a, y);
    bp = EFFECTS_ROW(b, y);
    dp = EFFECTS_ROW(dst, y);

    for (x = 0; x < a->size.w; x++)
    {
      uint32_t out;
      int      shift;

      out = 0;
      for (shift = 0; shift < 32; shift += 8)
      {
        uint32_t ca;
        uint32_t cb;

        ca = (ap[x] >> shift) & 0xFF;
        cb = (bp[x] >> shift) & 0xFF;

        out |= ((ca * calpha + cb * alpha) >> 8) << shift;
      }

      dp[x] = out;
    }
  }

  return result_OK;
}
