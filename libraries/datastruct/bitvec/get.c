/* --------------------------------------------------------------------------
 *    Name: get.c
 * Purpose: Bit vectors
 * ----------------------------------------------------------------------- */

#include "datastruct/bitvec.h"

#include "impl.h"

int bitvec_get(const bitvec_t *v, int bit)
{
  int word;

  word = bit >> 5;

  if (word >= v->length)
    return 0;

  return (v->vec[word] & (1u << (bit & 0x1F))) != 0;
}
