/* create.c -- hash */

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/suppress.h"

#include "base/result.h"
#include "utils/primes.h"

#include "datastruct/hash.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

/* Basic string hash. Retained for reference. */
/*static unsigned int string_hash(const void *a)
{
  const char  *s = a;
  unsigned int h;

  h = 0;
  while (*s)
    h = h * 37 + *s++;

  return h;
}*/

/* Fowler/Noll/Vo FNV-1 hash */
static unsigned int string_hash(const void *a)
{
  const char  *s = a;
  unsigned int h;

  h = 0x811c9dc5;
  while (*s)
  {
    h += (h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24);
    h ^= *s++;
  }

  return h;
}

static int string_compare(const void *a, const void *b)
{
  const char *sa = a;
  const char *sb = b;

  return strcmp(sa, sb);
}

static void string_destroy(void *string)
{
  free(string);
}

/* ----------------------------------------------------------------------- */

void hash_no_destroy_key(void *string)
{
  NOT_USED(string);
}

void hash_no_destroy_value(void *string)
{
  NOT_USED(string);
}

/* ----------------------------------------------------------------------- */

result_t hash_create(int                 nbins,
                     hash_fn            *fn,
                     hash_compare       *compare,
                     hash_destroy_key   *destroy_key,
                     hash_destroy_value *destroy_value,
                     hash_t            **ph)
{
  hash_t       *h;
  hash_node_t **bins;

  h = malloc(sizeof(*h));
  if (h == NULL)
    return result_OOM;

  nbins = prime_nearest(nbins);

  bins = calloc(nbins, sizeof(*h->bins));
  if (bins == NULL)
  {
    free(h);
    return result_OOM;
  }

  /* the default routines handle strings */

  h->hash_fn       = fn            ? fn            : string_hash;
  h->compare       = compare       ? compare       : string_compare;
  h->destroy_key   = destroy_key   ? destroy_key   : string_destroy;
  h->destroy_value = destroy_value ? destroy_value : string_destroy;
  h->nbins         = nbins;
  h->bins          = bins;

  *ph = h;

  return result_OK;
}
