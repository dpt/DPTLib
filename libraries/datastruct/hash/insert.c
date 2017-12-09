/* insert.c -- hash */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"

#include "datastruct/hash.h"

#include "impl.h"

result_t hash_insert(hash_t *h, const void *key, const void *value)
{
  hash_node_t **n;

  n = hash_lookup_node(h, key);
  if (*n)
  {
    /* already exists: update the value */

    h->destroy_value((void *)(*n)->value);  /* must cast away const */

    (*n)->value = value;

    h->destroy_key((void *) key); /* must cast away const */
  }
  else
  {
    hash_node_t *m;

    /* not found: create new node */

    m = malloc(sizeof(*m));
    if (m == NULL)
      return result_OOM;

    m->next  = NULL;
    m->key   = key;
    m->value = value;

    h->count++;

    *n = m;
  }

  return result_OK;
}
