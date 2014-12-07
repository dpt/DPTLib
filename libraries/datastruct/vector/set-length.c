/* --------------------------------------------------------------------------
 *    Name: set-length.c
 * Purpose: Vector - flexible array
 * ----------------------------------------------------------------------- */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "utils/barith.h"

#include "datastruct/vector.h"

#include "impl.h"

result_t vector_set_length(vector_t *v, size_t length)
{
  void *newbase;

  newbase = realloc(v->base, length * v->width);
  if (newbase == NULL)
    return result_OOM;

  v->used      = length;
  v->allocated = length;
  v->base      = newbase;

  return result_OK;
}
