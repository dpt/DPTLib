/* ensure.c -- vector - flexible array */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "datastruct/vector.h"

#include "impl.h"

result_t vector_ensure(vector_t *v, unsigned int need)
{
  if (need > v->allocated)
  {
    unsigned int required;
    void        *newbase;
    
    required = need;
    
    newbase = realloc(v->base, required * v->width);
    if (newbase == NULL)
      return result_OOM;

    // Note: We're not wiping the new portion of the array.
    
    v->allocated = required;
    v->base      = newbase;
  }
  
  return result_OK;
}
