/* set-all.c -- bit vectors */

#include <string.h>

#include "datastruct/bitvec.h"

#include "impl.h"

void bitvec_set_all(bitvec_t *v)
{
  // this is probably doing the wrong thing by setting all 'known' bits.
  // what you'd expect is that every single bit (including those unallocated)
  // would now be returned as one, so we'd need a flag to say what to read
  // unallocated bits as

  /* Note: This may set more bits than strictly required. */

  memset(v->vec, 0xFF, v->length * BYTESPERWORD);
}
