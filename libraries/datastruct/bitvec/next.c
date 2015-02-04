/* next.c -- bit vectors */

#include "utils/barith.h"
#include "datastruct/bitvec.h"

#include "impl.h"

int bitvec_next(const bitvec_t *v, int n)
{
  unsigned int hi;
  int          lo;

  if (!v->vec)
    return -1; /* no vec allocated */

  if (n >= 0)
  {
    hi = n >> LOG2BITSPERWORD;
    lo = n & WORDMASK;

    if (lo == WORDMASK) /* at word boundary: skip first step */
    {
      hi++;
      lo = -1; /* -1 => no mask */
    }
  }
  else
  {
    hi = 0;
    lo = -1;
  }

  for (; hi < v->length; hi++)
  {
    unsigned int word;
    unsigned int bits;

    word = v->vec[hi];
    if (word == 0)
      continue; /* no bits set */

    if (lo >= 0)
      word &= ~((1 << (lo + 1)) - 1);

    bits = LSB(word);
    if (bits)
      return (hi << LOG2BITSPERWORD) + ctz(bits);

    lo = -1; /* don't mask the next words */
  }

  return -1; /* ran out of words */
}
