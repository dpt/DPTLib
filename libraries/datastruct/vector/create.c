/* --------------------------------------------------------------------------
 *    Name: create.c
 * Purpose: Vector - flexible array
 * ----------------------------------------------------------------------- */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

vector_t *vector_create(size_t width)
{
  vector_t *v;

  v = calloc(1, sizeof(*v));
  if (v == NULL)
    return NULL;

  v->width = width;

  return v;
}
