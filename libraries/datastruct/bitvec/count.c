/* count.c -- bit vectors */

#include "utils/barith.h"
#include "datastruct/bitvec.h"

#include "impl.h"

unsigned int bitvec_count(const bitvec_t *v)
{
  unsigned int c;
  unsigned int i;

  c = 0;
  for (i = 0; i < v->length; i++)
    c += bitvec_countbits(v->vec[i]);

  return c;
}
