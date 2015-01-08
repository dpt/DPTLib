/* set.c -- vector - flexible array */

#include <assert.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "utils/minmax.h"

#include "datastruct/vector.h"

#include "impl.h"

void vector_set(vector_t *vector, int index, const void *value)
{
  assert(index < vector->allocated);

  /* Silently ignore requests to assign to non-existent entries. */
  if (index >= vector->allocated)
    return;

  memcpy(VECTOR_INDEX(vector, index), value, vector->width);

  /* Set 'used' to indicate the next available slot. */
  vector->used = MAX(vector->used, index + 1);
}
