/* insert.c -- hash */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"

#include "datastruct/hash.h"

#include "impl.h"

result_t hash_insert(hash_t *h, void *key, void *value)
{
  node **n;

  n = hash_lookup_node(h, key);
  if (*n)
  {
    /* already exists: update the value */

    h->destroy_value((*n)->value);

    (*n)->value = value;

    h->destroy_key(key);
  }
  else
  {
    node *m;

    /* not found: create new node */

    m = malloc(sizeof(*m));
    if (m == NULL)
      return result_OOM;

    m->next  = NULL;
    m->key   = key;
    m->value = value;

    *n = m;
  }

  return result_OK;
}
