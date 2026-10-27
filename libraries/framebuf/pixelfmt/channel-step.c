/* framebuf/pixelfmt/channel-step.c -- gap between a format's channel levels */

#include "framebuf/pixelfmt.h"

int pixelfmt_channel_step(pixelfmt_t fmt, int channel)
{
  switch (fmt)
  {
  case pixelfmt_bgrx4444:
  case pixelfmt_rgbx4444:
  case pixelfmt_xbgr4444:
  case pixelfmt_xrgb4444:
    return 16;

  case pixelfmt_bgrx5551:
  case pixelfmt_rgbx5551:
  case pixelfmt_xbgr1555:
  case pixelfmt_xrgb1555:
    return 8;

  case pixelfmt_bgr565:
  case pixelfmt_rgb565:
    return channel == 1 ? 4 : 8;

  case pixelfmt_y8:
  case pixelfmt_bgrx8888:
  case pixelfmt_rgbx8888:
  case pixelfmt_xbgr8888:
  case pixelfmt_xrgb8888:
  case pixelfmt_bgra8888:
  case pixelfmt_rgba8888:
  case pixelfmt_abgr8888:
  case pixelfmt_argb8888:
    return 1;

  default:
    return 0;
  }
}
