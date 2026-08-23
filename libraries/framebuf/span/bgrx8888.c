/* bgrx8888.c */

#include <string.h>

#include "framebuf/pixelfmt.h"

#include "framebuf/span.h"

#include "framebuf/span-bgrx8888.h"

#include "all8888.h"

#define RED_SHIFT   16
#define GREEN_SHIFT 8
#define BLUE_SHIFT  0
#define X_SHIFT     24

#include "all8888-generic.c"

SPAN_ALL8888_BLEND_CONST(span_bgrx8888_blendconst, pixelfmt_bgrx8888_t)
SPAN_ALL8888_BLEND_ARRAY(span_bgrx8888_blendarray, pixelfmt_bgrx8888_t)

const span_t span_bgrx8888 =
{
  pixelfmt_bgrx8888,
  span_all8888_copy,
  span_bgrx8888_blendconst,
  span_bgrx8888_blendarray,
};
