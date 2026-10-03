/* framebuf/effects/sharpen.c -- sharpen effect */

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"

#include "framebuf/effects.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

result_t effects_sharpen_apply(const bitmap_t *src, bitmap_t *dst)
{
  static const float kernel[] = { 1.5f, -0.25f };

  result_t        rc;
  convolve_lut_t *lut;

  rc = effects__convolve_init(kernel, NELEMS(kernel), &lut);
  if (rc != result_OK)
    return rc;

  rc = effects__convolve(lut, src, dst);

  effects__convolve_destroy(lut);

  return rc;
}
