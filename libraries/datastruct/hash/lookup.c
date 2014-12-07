/* --------------------------------------------------------------------------
 *    Name: lookup.c
 * Purpose: Hash
 * ----------------------------------------------------------------------- */

#include <stdlib.h>

#include "datastruct/hash.h"

#include "impl.h"

void *hash_lookup(hash_t *h, const void *key)
{
  node **n;

  n = hash_lookup_node(h, key);

  return (*n != NULL) ? (*n)->value : NULL;
}
