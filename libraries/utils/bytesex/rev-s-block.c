/* rev-s-block.c -- reversing bytesex */

#include <assert.h>

#include "utils/bytesex.h"

void rev_s_block(unsigned short int *array, size_t nelems)
{
  unsigned short int *p;
  unsigned int       *q;
  size_t              n;
  unsigned int        mask;

  assert(array != NULL);

  if (nelems == 0)
    return;

  p = array;

  /* align to a 4-byte boundary */
  if (((int) p & 3) != 0)
  {
    unsigned short int r0;

    r0 = *p;

    *p++ = (unsigned short int) ((r0 >> 8) | (r0 << 8));

    nelems--;
  }

  q = (void *) p; /* work around increase in alignment */

  n = (nelems >> 1) + 1; /* watch the birdie */

  mask = 0xff00ffffU; /* prepare the convert-o-tron */

  /* now do pairs at a time */
  while (--n != 0)
  {
    unsigned int r0, r1;

    r0 = *q;

    r1  = mask & (r0 << 8);
    r0 &= ~(mask >> 8);
    r0  = r1 | (r0 >> 8);

    *q++ = r0;
  }

  p = (void *) q;

  /* deal with trailer, if any */
  if ((nelems & 1) != 0)
  {
    unsigned short int r0;

    r0 = *p;

    *p = (unsigned short int) ((r0 >> 8) | (r0 << 8));
  }
}
