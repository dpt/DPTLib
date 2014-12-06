/* rev-l-block.c -- reversing bytesex */

#include <assert.h>

#include "utils/bytesex.h"

#include "util.h"

void rev_l_block(unsigned int *array, size_t nelems)
{
  unsigned int *p;
  unsigned int  mask;

  assert(array != NULL);

  p = array;

  mask = 0xffff00ffU;

  while (nelems-- != 0)
  {
    unsigned int r0, r1;

    r0 = *p;

    r1 = r0 ^ ROR(int, r0, 16);
    r1 = mask & (r1 >> 8);
    r0 = r1 ^ ROR(int, r0, 8);

    *p++ = r0;
  }
}
