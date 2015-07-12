/* get.c -- bit vectors */

#include "datastruct/bitvec.h"

#include "impl.h"

int bitvec_get(const bitvec_t *v, bitvec_index_t bit)
{
  unsigned int word;

  word = bit >> LOG2BITSPERWORD;

  if (word >= v->length)
    return 0;

  return (v->vec[word] & (bitvec_1 << (bit & WORDMASK))) != 0;
}
