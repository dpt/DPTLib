/* framebuf/span/rgb565.c */

#include "framebuf/pixelfmt.h"

#include "framebuf/span.h"

#include "framebuf/span-rgb565.h"

#include "all16bpp.h"

#define RED_SHIFT   PIXELFMT_Rxx565_SHIFT
#define GREEN_SHIFT PIXELFMT_xGx565_SHIFT
#define BLUE_SHIFT  PIXELFMT_xxB565_SHIFT
#define RED_MASK    PIXELFMT_Rxx565_MASK
#define GREEN_MASK  PIXELFMT_xGx565_MASK
#define BLUE_MASK   PIXELFMT_xxB565_MASK

#include "all16bpp-generic.c"

SPAN_ALL16BPP_BLEND_CONST(span_rgb565_blendconst, pixelfmt_rgb565_t)
SPAN_ALL16BPP_BLEND_ARRAY(span_rgb565_blendarray, pixelfmt_rgb565_t)

const span_t span_rgb565 =
{
  pixelfmt_rgb565,
  span_all16bpp_copy,
  span_all16bpp_fill,
  span_rgb565_blendconst,
  span_rgb565_blendarray,
};
