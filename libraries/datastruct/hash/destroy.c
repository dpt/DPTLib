/* destroy.c -- hash */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/hash.h"

#include "impl.h"

void hash_destroy(hash_t *h)
{
  unsigned int i;

  for (i = 0; i < h->nbins; i++)
    while (h->bins[i])
      hash_remove_node(h, &h->bins[i]);

  free(h->bins);

  free(h);
}
