/* impl.c -- hash */

#ifndef DATASTRUCT_HASH_IMPL_H
#define DATASTRUCT_HASH_IMPL_H

#include "datastruct/hash.h"

typedef struct hash_node
{
  struct hash_node      *next;
  void                  *key;
  void                  *value;
}
hash_node_t;

struct hash
{
  hash_fn_t             *hash_fn;
  hash_compare_t        *compare;
  hash_destroy_key_t    *destroy_key;
  hash_destroy_value_t  *destroy_value;
  unsigned int           nbins;
  hash_node_t          **bins;
};

hash_node_t **hash_lookup_node(hash_t *h, const void *key);
void hash_remove_node(hash_t *h, hash_node_t **n);

#endif /* DATASTRUCT_HASH_IMPL_H */
