/* xbgr8888.c */

#include <string.h>

#include "framebuf/pixelfmt.h"

#include "framebuf/span.h"

#include "framebuf/span-xbgr8888.h"

#include "all8888.h"

#define RED_SHIFT   0
#define GREEN_SHIFT 8
#define BLUE_SHIFT  16
#define X_SHIFT     24

#include "all8888-generic.c"

SPAN_ALL8888_BLEND_CONST(span_xbgr8888_blendconst, pixelfmt_xbgr8888_t)
SPAN_ALL8888_BLEND_ARRAY(span_xbgr8888_blendarray, pixelfmt_xbgr8888_t)

const span_t span_xbgr8888 =
{
  pixelfmt_xbgr8888,
  span_all8888_copy,
  span_xbgr8888_blendconst,
  span_xbgr8888_blendarray,
};
