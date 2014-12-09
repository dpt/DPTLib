/* toggle.c -- bit vectors */

#include "base/result.h"

#include "datastruct/bitvec.h"

#include "impl.h"

result_t bitvec_toggle(bitvec_t *v, int bit)
{
  result_t err;
  int      word;

  word = bit >> 5;

  err = bitvec_ensure(v, word + 1);
  if (err)
    return err;

  v->vec[word] ^= 1u << (bit & 0x1F);

  return result_OK;
}
