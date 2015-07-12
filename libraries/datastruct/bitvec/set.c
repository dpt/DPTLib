/* set.c -- bit vectors */

#include "base/result.h"

#include "datastruct/bitvec.h"

#include "impl.h"

result_t bitvec_set(bitvec_t *v, bitvec_index_t bit)
{
  result_t     err;
  unsigned int word;

  word = bit >> LOG2BITSPERWORD;

  err = bitvec_ensure(v, word + 1);
  if (err)
    return err;

  v->vec[word] |= bitvec_1 << (bit & WORDMASK);

  return result_OK;
}
