/* lookup.c -- hash */

#include <stdlib.h>

#include "datastruct/hash.h"

#include "impl.h"

const void *hash_lookup(hash_t *h, const void *key)
{
  hash_node_t **n;

  n = hash_lookup_node(h, key);

  return (*n != NULL) ? (*n)->value : h->default_value;
}
