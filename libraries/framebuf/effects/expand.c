/* framebuf/effects/expand.c -- stretch the used range to full contrast */

#include <stdint.h>

#include "base/result.h"
#include "framebuf/bitmap.h"

#include "framebuf/effects.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

/* FIXME: Threshold should use a percentage rather than literal count. */

result_t effects_expand_apply(const bitmap_t *src,
                              bitmap_t       *dst,
                              unsigned int    threshold)
{
  result_t         rc;
  effects_layout_t layout;
  unsigned int     hist[3][256];
  int              min;
  int              max;
  int              i;
  int              j;
  uint8_t          lut[256];

  rc = effects__check(src, dst, &layout);
  if (rc != result_OK)
    return rc;

  effects__histogram(src, &layout, hist);

  /* find the minima and maxima for all of the R,G,B histograms */

  min = 255;
  max = 0;

  for (i = 0; i < 3; i++)
  {
    for (j = 0; j < 256; j++)
      if (hist[i][j] > threshold)
        break;

    if (j < min)
      min = j;

    for (j = 255; j >= 0; j--)
      if (hist[i][j] > threshold)
        break;

    if (j > max)
      max = j;
  }

  if ((min == 0 && max == 255) || min >= max)
  {
    effects__copy(src, dst);

    return result_OK;
  }

  /* min,max are the first,last levels used */

  for (j = 0; j < 256; j++)
  {
    if (j <= min)
      lut[j] = 0;
    else if (j >= max)
      lut[j] = 255;
    else
      lut[j] = (uint8_t) ((j - min) * 255 / (max - min));
  }

  effects__remap(src, dst, &layout, lut, lut, lut, NULL);

  return result_OK;
}
