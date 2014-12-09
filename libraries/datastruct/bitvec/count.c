/* count.c -- bit vectors */

#include "utils/barith.h"
#include "datastruct/bitvec.h"

#include "impl.h"

int bitvec_count(const bitvec_t *v)
{
  int c;
  int i;

  c = 0;
  for (i = 0; i < v->length; i++)
    c += countbits(v->vec[i]);

  return c;
}
