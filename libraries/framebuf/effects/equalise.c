/* framebuf/effects/equalise.c -- histogram equalisation */

#include <stdint.h>

#include "base/result.h"
#include "framebuf/bitmap.h"

#include "framebuf/effects.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

result_t effects_equalise_apply(const bitmap_t *src, bitmap_t *dst)
{
  result_t         rc;
  effects_layout_t layout;
  unsigned int     hist[3][256];
  uint8_t          lut[3][256];
  int              i;

  rc = effects__check(src, dst, &layout);
  if (rc != result_OK)
    return rc;

  if (src->size.w == 0 || src->size.h == 0)
    return result_OK;

  effects__histogram(src, &layout, hist);

  for (i = 0; i < 3; i++)
  {
    uint64_t total;
    uint64_t cumulative;
    int      j;

    total = (uint64_t) src->size.w * (uint64_t) src->size.h;

    /* the cumulative histogram, scaled, is the map */

    cumulative = 0;
    for (j = 0; j < 256; j++)
    {
      cumulative += hist[i][j];
      lut[i][j] = (uint8_t) (cumulative * 255 / total);
    }
  }

  effects__remap(src, dst, &layout, lut[0], lut[1], lut[2], NULL);

  return result_OK;
}
