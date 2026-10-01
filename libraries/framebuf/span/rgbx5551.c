/* framebuf/span/rgbx5551.c */

#include "framebuf/pixelfmt.h"

#include "framebuf/span.h"

#include "framebuf/span-rgbx5551.h"

#include "all16bpp.h"

#define RED_SHIFT   PIXELFMT_Rxxx5551_SHIFT
#define GREEN_SHIFT PIXELFMT_xGxx5551_SHIFT
#define BLUE_SHIFT  PIXELFMT_xxBx5551_SHIFT
#define RED_MASK    PIXELFMT_Rxxx5551_MASK
#define GREEN_MASK  PIXELFMT_xGxx5551_MASK
#define BLUE_MASK   PIXELFMT_xxBx5551_MASK

#include "all16bpp-generic.c"

SPAN_ALL16BPP_BLEND_CONST(span_rgbx5551_blendconst, pixelfmt_rgbx5551_t)
SPAN_ALL16BPP_BLEND_ARRAY(span_rgbx5551_blendarray, pixelfmt_rgbx5551_t)

const span_t span_rgbx5551 =
{
  pixelfmt_rgbx5551,
  span_all16bpp_copy,
  span_all16bpp_fill,
  span_rgbx5551_blendconst,
  span_rgbx5551_blendarray,
};
