/* --------------------------------------------------------------------------
 *    Name: destroy.c
 * Purpose: Bit vectors
 * ----------------------------------------------------------------------- */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/bitvec.h"

#include "impl.h"

void bitvec_destroy(bitvec_t *v)
{
  if (v == NULL)
    return;

  free(v->vec);
  free(v);
}
