/* eq.c -- bit vectors */

#include <string.h>

#include "utils/minmax.h"

#include "datastruct/bitvec.h"

#include "impl.h"

/* If a bitvec had previously had a high bit set, then cleared it, then the
 * high allocated words may all be zero.
 *
 * e.g. using nibbles for words it's like this:
 *
 *   0100 1000 0000 0111
 *
 * Then it has its MSB unset:
 *
 *   0000 1000 0000 0111
 *
 * So the top words are zero. Now we want to compare against some other word
 * with possibly the same situation:
 *
 *        1000 0000 0111
 *
 * So they're not the same length, but do have the same bits set.
 */

int bitvec_eq(const bitvec_t *a, const bitvec_t *b)
{
  unsigned int    al;
  unsigned int    bl;
  unsigned int    l;
  const bitvec_t *p; /* the longer one */

  al = a->length;
  bl = b->length;

  l = MIN(al, bl);

  if (l > 0) /* space for bits allocated */
  {
    int r;

    r = memcmp(a->vec, b->vec, l * BYTESPERWORD);
    if (r != 0)
      return 0; /* not equal */
  }

  if (al == bl)
    return 1; /* equal */

  p = (al == l) ? b : a; /* pick the longer one */

  for (; l < p->length; l++)
    if (p->vec[l])
      return 0; /* nonzero bits found - not equal */

  return 1; /* all remaining words were zero - equal */
}
