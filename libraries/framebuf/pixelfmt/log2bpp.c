/* log2bpp.c -- given a pixel format return its log2 bits-per-pixel size */

#include <assert.h>

#include "framebuf/pixelfmt.h"

int pixelfmt_log2bpp(pixelfmt_t fmt)
{
  switch (fmt)
  {
  case pixelfmt_p1:
    return 0;
  case pixelfmt_p2:
    return 1;
  case pixelfmt_p4:
    return 2;
  case pixelfmt_p8:
  case pixelfmt_y8:
    return 3;

  case pixelfmt_bgrx4444:
  case pixelfmt_rgbx4444:
  case pixelfmt_xbgr4444:
  case pixelfmt_xrgb4444:
  case pixelfmt_bgrx5551:
  case pixelfmt_rgbx5551:
  case pixelfmt_xbgr1555:
  case pixelfmt_xrgb1555:
  case pixelfmt_bgr565:
  case pixelfmt_rgb565:
    return 4;

  case pixelfmt_bgrx8888:
  case pixelfmt_rgbx8888:
  case pixelfmt_xbgr8888:
  case pixelfmt_xrgb8888:
  case pixelfmt_bgra8888:
  case pixelfmt_rgba8888:
  case pixelfmt_abgr8888:
  case pixelfmt_argb8888:
    return 5;

  default:
    assert(0);
    return -1;
  }
}
