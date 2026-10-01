/* framebuf/span/all16bpp.c */

#include <string.h>

#include "all16bpp.h"

void span_all16bpp_copy(void *dst, const void *src, int length)
{
  if (dst == src) /* screen-to-screen copy is a no-op */
    return;

  memcpy(dst, src, (size_t) length * 2);
}

void span_all16bpp_fill(void          *dst,
                        int            first,
                        pixelfmt_any_t pixel,
                        int            length)
{
  pixelfmt_any16_t *p;

  p = (pixelfmt_any16_t *) dst + first;
  while (length--)
    *p++ = (pixelfmt_any16_t) pixel;
}
