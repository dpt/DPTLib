/* walk.c -- hash */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"

#include "datastruct/hash.h"

#include "impl.h"

result_t hash_walk(const hash_t *h, hash_walk_callback_t *cb, void *cbarg)
{
  unsigned int i;

  for (i = 0; i < h->nbins; i++)
  {
    hash_node_t *n;
    hash_node_t *next;

    for (n = h->bins[i]; n != NULL; n = next)
    {
      result_t r;

      next = n->next;

      r = cb(n->key, n->value, cbarg);
      if (r)
        return r;
    }
  }

  return result_OK;
}
