/* datastruct/bitvec/create.c -- bit vectors */

#include <assert.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"

#include "datastruct/bitvec.h"

#include "impl.h"

bitvec_t *bitvec_create(const alloc_t *alloc, unsigned int length)
{
  result_t  err;
  bitvec_t *v;

  if (alloc == NULL)
    alloc = &alloc_default;

  v = alloc->malloc(sizeof(*v));
  if (v == NULL)
    return NULL;

  v->alloc  = alloc;
  v->length = 0;
  v->vec    = NULL;

  err = bitvec_ensure(v, length >> LOG2BITSPERWORD);
  if (err)
  {
    assert(err == result_OOM);
    alloc->free(v);
    return NULL;
  }

  return v;
}
