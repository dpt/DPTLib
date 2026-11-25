/* datastruct/vector/destroy.c -- flexible array */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

void vector_destroy(vector_t *doomed)
{
  if (doomed == NULL)
    return;

  doomed->alloc->free(doomed->base);
  doomed->alloc->free(doomed);
}
