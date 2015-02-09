/* set-length.c -- vector - flexible array */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

result_t vector_set_length(vector_t *v, unsigned int length)
{
  void *newbase;

  newbase = realloc(v->base, length * v->width);
  if (newbase == NULL)
    return result_OOM;

  v->used      = length; // FIXME: Looks wrong.
  v->allocated = length;
  v->base      = newbase;

  return result_OK;
}
