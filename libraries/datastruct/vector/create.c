/* datastruct/vector/create.c -- flexible array */

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

vector_t *vector_create(const alloc_t *alloc, size_t width)
{
  vector_t *v;

  if (alloc == NULL)
    alloc = &alloc_default;

  v = alloc->malloc(sizeof(*v));
  if (v == NULL)
    return NULL;

  memset(v, 0, sizeof(*v));
  v->alloc = alloc;
  v->width = width;

  return v;
}
