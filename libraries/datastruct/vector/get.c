/* get.c -- vector - flexible array */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

void *vector_get(vector_t *v, int i)
{
  return (char *) v->base + i * v->width;
}
