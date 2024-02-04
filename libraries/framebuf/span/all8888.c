/* all8888.c */

#include <string.h>

#include "all8888.h"

void span_all8888_copy(void *dst, const void *src, int length)
{
  if (dst == src) /* screen-to-screen copy is a no-op */
    return;

  memcpy(dst, src, length * 4);
}
