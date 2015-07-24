/* count.c -- hash */

#include "datastruct/hash.h"

#include "impl.h"

int hash_count(hash_t *hash)
{
  return hash->count;
}
