/* lookup-node.c -- hash */

#include <stdlib.h>

#include "datastruct/hash.h"

#include "impl.h"

hash_node_t **hash_lookup_node(hash_t *h, const void *key)
{
  int           hash;
  hash_node_t **n;

  hash = h->hash_fn(key) % h->nbins;
  for (n = &h->bins[hash]; *n != NULL; n = &(*n)->next)
    if (h->compare(key, (*n)->key) == 0)
      break;

  return n;
}
