/* framebuf/span/all8888.c */

#include <string.h>

#include "all8888.h"

void span_all8888_copy(void *dst, const void *src, int length)
{
  if (dst == src) /* screen-to-screen copy is a no-op */
    return;

  memcpy(dst, src, length * 4);
}

void span_all8888_fill(void          *dst,
                       int            first,
                       pixelfmt_any_t pixel,
                       int            length)
{
  pixelfmt_any32_t *p;

  p = (pixelfmt_any32_t *) dst + first;
  while (length--)
    *p++ = pixel;
}
