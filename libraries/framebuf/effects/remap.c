/* framebuf/effects/remap.c -- per-channel lookup remapping and histograms */

#include <stdint.h>

#include "base/result.h"
#include "framebuf/bitmap.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

void effects__remap(const bitmap_t         *src,
                    bitmap_t               *dst,
                    const effects_layout_t *layout,
                    const uint8_t          *red,
                    const uint8_t          *green,
                    const uint8_t          *blue,
                    const uint8_t          *alpha)
{
  int          x, y;
  unsigned int amask;

  amask = EFFECTS_ALPHA_MASK(layout);

  for (y = 0; y < src->size.h; y++)
  {
    const uint32_t *sp;
    uint32_t       *dp;

    sp = EFFECTS_ROW(src, y);
    dp = EFFECTS_ROW(dst, y);

    for (x = 0; x < src->size.w; x++)
    {
      uint32_t p;
      uint32_t out;

      p = sp[x];

      out = ((uint32_t) red[(p >> layout->r) & 0xFF] << layout->r) |
            ((uint32_t) green[(p >> layout->g) & 0xFF] << layout->g) |
            ((uint32_t) blue[(p >> layout->b) & 0xFF] << layout->b);

      if (alpha)
        out |= (uint32_t) alpha[(p >> layout->a) & 0xFF] << layout->a;
      else
        out |= p & amask;

      dp[x] = out;
    }
  }
}

/* ----------------------------------------------------------------------- */

void effects__histogram(const bitmap_t         *src,
                        const effects_layout_t *layout,
                        unsigned int            hist[3][256])
{
  int x, y;
  int i;

  for (i = 0; i < 256; i++)
    hist[0][i] = hist[1][i] = hist[2][i] = 0;

  for (y = 0; y < src->size.h; y++)
  {
    const uint32_t *sp;

    sp = EFFECTS_ROW(src, y);

    for (x = 0; x < src->size.w; x++)
    {
      hist[0][(sp[x] >> layout->r) & 0xFF]++;
      hist[1][(sp[x] >> layout->g) & 0xFF]++;
      hist[2][(sp[x] >> layout->b) & 0xFF]++;
    }
  }
}
