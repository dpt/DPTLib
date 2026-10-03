/* framebuf/effects/blur.c -- blur effect */

#include <stdlib.h>

#include "base/result.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"

#include "framebuf/effects.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

static void make_kernel(effects_blur_method_t method,
                        int                   width,
                        float                *kernel)
{
  double k[effects_blur_MAX_AMOUNT];
  double sum;
  int    i;

  switch (method)
  {
  case effects_blur_BOX:
    for (i = 0; i < width; i++)
      k[i] = 1.0;
    break;

  case effects_blur_GAUSSIAN:
    for (i = 0; i < width; i++)
    {
      double f;

      /* 2f^3 - 3f^2 + 1 */
      f = (double) i / width;
      k[i] = (float) (((2.0 * f * f * f) - (3.0 * f * f) + 1.0) / width);
    }
    break;
  }

  /* sum the weights */

  sum = k[0];
  for (i = 1; i < width; i++)
    sum += k[i] * 2;

  /* scale the kernel */

  for (i = 0; i < width; i++)
    kernel[i] = (float) (k[i] / sum);
}

result_t effects_blur_apply(const bitmap_t       *src,
                            bitmap_t             *dst,
                            effects_blur_method_t method,
                            int                   amount)
{
  result_t        rc;
  float           kernel[effects_blur_MAX_AMOUNT];
  convolve_lut_t *lut;

  if (method != effects_blur_BOX && method != effects_blur_GAUSSIAN)
    return result_BAD_ARG;

  amount = CLAMP(amount, effects_blur_MIN_AMOUNT, effects_blur_MAX_AMOUNT);

  make_kernel(method, amount, kernel);

  rc = effects__convolve_init(kernel, amount, &lut);
  if (rc != result_OK)
    return rc;

  rc = effects__convolve(lut, src, dst);

  effects__convolve_destroy(lut);

  return rc;
}
