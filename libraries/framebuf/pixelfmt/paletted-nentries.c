/* framebuf/pixelfmt/paletted-nentries.c -- palette entry count for a packed paletted format */

#include "framebuf/pixelfmt.h"

int pixelfmt_paletted_nentries(pixelfmt_t fmt)
{
  switch (fmt)
  {
  case pixelfmt_p1:
    return 2;
  case pixelfmt_p2:
    return 4;
  case pixelfmt_p4:
    return 16;
  case pixelfmt_p8:
    return 256;

  default:
    return 0;
  }
}
