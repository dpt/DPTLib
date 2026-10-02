/* datastruct/bitarr/count.c -- arrays of bits */

#include <stddef.h>

#include "utils/barith.h"
#include "datastruct/bitarr.h"

int bitarr_count(const bitarr_t *arr, size_t bytelen)
{
  const bitarr_elem_t *base;
  const bitarr_elem_t *end;
  int                  c;
  const unsigned char *tail;
  const unsigned char *tailend;

  base = arr->entries;
  end  = base + bytelen / sizeof(*base);

  c = 0;
  while (base != end)
    c += countbits(*base++);

  /* any bytes past the last whole element */
  tail    = (const unsigned char *) end;
  tailend = (const unsigned char *) arr->entries + bytelen;
  while (tail != tailend)
    c += countbits(*tail++);

  return c;
}
