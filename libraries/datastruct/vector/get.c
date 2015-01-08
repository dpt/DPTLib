/* get.c -- vector - flexible array */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

void *vector_get(const vector_t *v, int index)
{
  return VECTOR_INDEX(v, index);
}
