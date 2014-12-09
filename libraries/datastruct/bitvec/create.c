/* --------------------------------------------------------------------------
 *    Name: create.c
 * Purpose: Bit vectors
 * ----------------------------------------------------------------------- */

#include <assert.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"

#include "datastruct/bitvec.h"

#include "impl.h"

bitvec_t *bitvec_create(int length)
{
  result_t  err;
  bitvec_t *v;

  v = malloc(sizeof(*v));
  if (v == NULL)
    return NULL;

  v->length = 0;
  v->vec    = NULL;

  err = bitvec_ensure(v, length >> 5);
  if (err)
  {
    assert(err == result_OOM);
    return NULL;
  }

  return v;
}
