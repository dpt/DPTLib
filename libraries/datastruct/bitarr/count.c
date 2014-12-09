/* count.c -- arrays of bits */

#include <stddef.h>

#include "utils/barith.h"
#include "datastruct/bitarr.h"

int bitarr_count(const bitarr_t *arr, size_t bytelen)
{
  const bitarr_elem_t *base;
  const bitarr_elem_t *end;
  int                  c;

  base = arr->entries;
  end  = base + (bytelen >> (BITARR_SHIFT - 3)); // FIXME: This will round off any sub-word units...

  c = 0;
  while (base != end)
    c += countbits(*base++);

  return c;
}
