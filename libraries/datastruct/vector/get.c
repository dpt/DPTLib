/* get.c -- vector - flexible array */

#include <assert.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

void *vector_get(const vector_t *v, unsigned int index)
{
  assert(index < v->allocated);

  return VECTOR_INDEX(v, index);
}
