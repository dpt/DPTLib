/* clear.c -- bit vectors */

#include "datastruct/bitvec.h"

#include "impl.h"

void bitvec_clear(bitvec_t *v, bitvec_index_t bit)
{
  unsigned int word;

  word = bit >> 5;

  if (word >= v->length)
    return;

  v->vec[word] &= ~(1u << (bit & WORDMASK));
}
