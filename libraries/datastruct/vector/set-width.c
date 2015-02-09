/* set-width.c -- vector - flexible array */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "utils/barith.h"
#include "utils/array.h"

#include "datastruct/vector.h"

#include "impl.h"

result_t vector_set_width(vector_t *v, size_t width)
{
  void   *newbase;
  size_t  currsz;
  size_t  newsz;

  if (width == 0)
    return result_BAD_ARG;

  if (width == v->width)
    return result_OK;

  currsz = v->allocated * v->width;
  newsz  = v->used * width;

  /* Squeeze now, before shrinking the block. */
  if (width < v->width)
    array_squeeze(v->base, v->used, v->width, width); // 64-bit

  /* Avoid calling realloc for the same size block. */
  if (currsz != newsz)
  {
    newbase = realloc(v->base, newsz);
    if (newbase == NULL)
      return result_OOM;
  }
  else
  {
    newbase = v->base;
  }

  /* Stretch now that the block has been grown. */
  if (width > v->width)
    array_stretch(newbase, v->used, v->width, width, 0); // 64-bit

  v->width     = width;
  /* v->used remains the same */
  v->allocated = v->used;
  v->base      = newbase;

  return result_OK;
}
