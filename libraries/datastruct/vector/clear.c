/* --------------------------------------------------------------------------
 *    Name: clear.c
 * Purpose: Vector - flexible array
 * ----------------------------------------------------------------------- */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

void vector_clear(vector_t *v)
{
  v->used = 0;
}
