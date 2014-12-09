/* walk-cont.c -- hash */

#include <assert.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"

#include "datastruct/hash.h"

#include "impl.h"

result_t hash_walk_continuation(hash_t      *h,
                                int          continuation,
                                int         *nextcontinuation,
                                const void **key,
                                const void **value)
{
  unsigned int bin;
  unsigned int item;
  int          i;
  node        *n;
  node        *next = NULL;

  if (continuation == -1) /* previous iteration was the last element */
    return result_HASH_END;

  /* The continuation value is treated as a pair of 16-bit fields, the top
   * half being the bin and the bottom half being the node within the bin. */

  bin  = ((unsigned int) continuation & 0xffff0000) >> 16;
  item = ((unsigned int) continuation & 0x0000ffff) >> 0;

  if (bin >= h->nbins)
    return result_HASH_BAD_CONT; /* invalid continuation value */

  /* if we're starting off, scan forward to the first occupied bin */

  if (continuation == 0)
  {
    while (h->bins[bin] == NULL)
      bin++;

    if (bin == h->nbins)
      return result_HASH_END; /* all bins were empty */
  }

  i = 0; /* node counter */

  for (n = h->bins[bin]; n; n = next)
  {
    next = n->next;

    if (i == item)
      break;

    i++;
  }

  if (n == NULL) /* invalid continuation value */
    return result_HASH_BAD_CONT;

  *key   = n->key;
  *value = n->value;

  /* form the continuation value and return it */

  /* a chain this long would be very odd, but ... */
  assert(i + 1 <= 0xffff);

  if (next)
  {
    *nextcontinuation = (bin << 16) | (i + 1); /* current bin, next node */
    return result_OK;
  }

  /* scan forward to the next occupied bin */

  while (++bin < h->nbins)
    if (h->bins[bin])
    {
      *nextcontinuation = bin << 16; /* next occupied bin, first node */
      return result_OK;
    }

  *nextcontinuation = -1; /* ran out of bins - remember for next iter */

  return result_OK;
}
