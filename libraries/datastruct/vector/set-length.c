/* datastruct/vector/set-length.c -- flexible array */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

result_t vector_set_length(vector_t *v, unsigned int length)
{
  void *newbase;

  /* realloc of zero bytes may return NULL, or free the block, so release
   * the storage here instead */
  if (length == 0)
  {
    free(v->base);
    newbase = NULL;
  }
  else
  {
    newbase = realloc(v->base, length * v->width);
    if (newbase == NULL)
      return result_OOM;
  }

  v->used      = length;
  v->allocated = length;
  v->base      = newbase;

  return result_OK;
}
