/* framebuf/effects/layout.c -- 32bpp pixel layouts and argument checks */

#include <stddef.h>

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/pixelfmt.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

result_t effects__layout(pixelfmt_t fmt, effects_layout_t *layout)
{
  switch (fmt)
  {
  case pixelfmt_rgbx8888:
  case pixelfmt_rgba8888:
    layout->r =  0; layout->g =  8; layout->b = 16; layout->a = 24;
    break;

  case pixelfmt_bgrx8888:
  case pixelfmt_bgra8888:
    layout->b =  0; layout->g =  8; layout->r = 16; layout->a = 24;
    break;

  case pixelfmt_xbgr8888:
  case pixelfmt_abgr8888:
    layout->a =  0; layout->b =  8; layout->g = 16; layout->r = 24;
    break;

  case pixelfmt_xrgb8888:
  case pixelfmt_argb8888:
    layout->a =  0; layout->r =  8; layout->g = 16; layout->b = 24;
    break;

  default:
    return result_BAD_ARG;
  }

  return result_OK;
}

result_t effects__check_one(const bitmap_t   *src,
                            effects_layout_t *layout)
{
  result_t rc;

  if (src == NULL || src->base == NULL || src->rowbytes % 4 != 0)
    return result_BAD_ARG;

  rc = effects__layout(src->format, layout);
  if (rc != result_OK)
    return rc;

  if (src->size.w < 0 || src->size.h < 0 ||
      src->rowbytes < src->size.w * 4)
    return result_BAD_ARG;

  return result_OK;
}

result_t effects__check(const bitmap_t   *src,
                        const bitmap_t   *dst,
                        effects_layout_t *layout)
{
  result_t rc;

  rc = effects__check_one(src, layout);
  if (rc != result_OK)
    return rc;

  if (dst == NULL || dst->base == NULL || dst->rowbytes % 4 != 0)
    return result_BAD_ARG;

  if (dst->format != src->format ||
      dst->size.w != src->size.w || dst->size.h != src->size.h ||
      dst->rowbytes < dst->size.w * 4)
    return result_BAD_ARG;

  return result_OK;
}

/* ----------------------------------------------------------------------- */

void effects__copy(const bitmap_t *src, bitmap_t *dst)
{
  int x, y;

  if (src->base == dst->base && src->rowbytes == dst->rowbytes)
    return;

  for (y = 0; y < src->size.h; y++)
  {
    const uint32_t *sp;
    uint32_t       *dp;

    sp = EFFECTS_ROW(src, y);
    dp = EFFECTS_ROW(dst, y);

    for (x = 0; x < src->size.w; x++)
      dp[x] = sp[x];
  }
}
