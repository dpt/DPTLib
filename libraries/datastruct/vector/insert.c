/* insert.c -- vector - flexible array */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

result_t vector_insert(vector_t *v, const void *value)
{
  result_t     err;
  unsigned int used;
  void        *base;

  used = v->used;

  if ((err = vector_ensure(v, used + 1)) != result_OK)
    return err;

  base = vector_get(v, (int) used);
  assert(base != NULL); /* can't fail, so assert */

  memcpy(base, value, v->width);
  v->used++;

  return result_OK;
}

result_t vector_insert_many(vector_t *v, const void *values, int nvalues)
{
  result_t     err;
  unsigned int used;
  void        *base;

  used = v->used;

  if ((err = vector_ensure(v, used + nvalues)) != result_OK)
    return err;

  base = vector_get(v, (int) used);
  assert(base != NULL); /* can't fail, so assert */

  memcpy(base, values, v->width * nvalues);
  v->used += nvalues;

  return result_OK;
}
