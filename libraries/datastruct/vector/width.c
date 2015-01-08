/* width.c -- vector - flexible array */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

size_t vector_width(const vector_t *v)
{
  return v->width;
}
