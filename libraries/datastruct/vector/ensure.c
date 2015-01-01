/* ensure.c -- vector - flexible array */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

result_t vector_ensure(vector_t *v, size_t need)
{
  if (need > v->allocated)
  {
    size_t  required;
    void   *newbase;
    
    required = need;
    
    newbase = realloc(v->base, required * v->width);
    if (newbase == NULL)
      return result_OOM;
    
    v->allocated = required;
    v->base      = newbase;
  }
  
  return result_OK;
}