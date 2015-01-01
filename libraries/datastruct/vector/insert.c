/* insert.c -- vector - flexible array */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

result_t vector_insert(vector_t *v, const void *element)
{
  result_t  err;
  size_t    used;
  void     *base;
  
  used = v->used;
  
  if ((err = vector_ensure(v, used + 1)) != result_OK)
    return err;
  
  base = vector_get(v, (int) used);
  assert(base != NULL); /* can't fail, so assert */
  
  memcpy(base, element, v->width);
  v->used++;
  
  return result_OK;
}
