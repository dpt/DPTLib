/* clear.c -- bit vectors */

#include "datastruct/bitvec.h"

#include "impl.h"

void bitvec_clear(bitvec_t *v, bitvec_index_t bit)
{
  unsigned int word;

  word = bit >> LOG2BITSPERWORD;

  if (word >= v->length)
    return;

  v->vec[word] &= ~(bitvec_1 << (bit & WORDMASK));
}
