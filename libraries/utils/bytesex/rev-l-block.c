/* utils/bytesex/rev-l-block.c -- reversing bytesex */

#include <assert.h>

#include "utils/bytesex.h"

void rev_l_block(unsigned int *array, size_t nelems)
{
  unsigned int *p;

  assert(array != NULL);

  p = array;

  while (nelems-- != 0)
  {
    *p = rev_l(*p);
    p++;
  }
}
