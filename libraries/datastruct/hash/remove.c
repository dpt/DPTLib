/* remove.c -- hash */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/hash.h"

#include "impl.h"

void hash_remove_node(hash_t *h, hash_node_t **n)
{
  hash_node_t *doomed;

  doomed = *n;

  *n = doomed->next;

  h->destroy_key((void *) doomed->key);
  h->destroy_value((void *) doomed->value);

  free(doomed);

  h->count--;
}

void hash_remove(hash_t *h, const void *key)
{
  hash_node_t **n;

  n = hash_lookup_node(h, key);
  hash_remove_node(h, n);
}
