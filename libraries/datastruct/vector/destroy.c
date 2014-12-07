/* --------------------------------------------------------------------------
 *    Name: destroy.c
 * Purpose: Vector - flexible array
 * ----------------------------------------------------------------------- */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

void vector_destroy(vector_t *doomed)
{
  if (doomed == NULL)
    return;

  free(doomed->base);
  free(doomed);
}
