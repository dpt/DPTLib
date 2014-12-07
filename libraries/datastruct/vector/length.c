/* --------------------------------------------------------------------------
 *    Name: length.c
 * Purpose: Vector - flexible array
 * ----------------------------------------------------------------------- */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

int vector_length(const vector_t *v)
{
  return v->used;
}
