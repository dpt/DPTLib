/* length.c -- bit vectors */

#include <stddef.h>

#include "datastruct/bitvec.h"

#include "impl.h"

unsigned int bitvec_length(const bitvec_t *v)
{
  return v->length << LOG2BITSPERWORD;
}
