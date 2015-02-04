/* clear-all.c -- bit vectors */

#include <string.h>

#include "datastruct/bitvec.h"

#include "impl.h"

void bitvec_clear_all(bitvec_t *v)
{
  /* Note: This may clear more bits than strictly required. */

  memset(v->vec, 0x00, v->length * BYTESPERWORD);
}
