/* utils/array/squeeze.c */

#include <assert.h>
#include <string.h>

#include "utils/array.h"

/* walk forwards, skip first element */
void array_squeeze(unsigned char *base,
                   int            nelems,
                   size_t         oldwidth,
                   size_t         newwidth)
{
  unsigned char *end;
  unsigned char *p, *q;

  assert(oldwidth > newwidth);

  end = base + nelems * oldwidth;

  p = base + 1 * oldwidth;
  q = base + 1 * newwidth;

  while (p < end)
  {
    memmove(q, p, newwidth);

    p += oldwidth;
    q += newwidth;
  }
}
