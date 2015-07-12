/* ensure.c -- bit vectors */

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"

#include "datastruct/bitvec.h"

#include "impl.h"

result_t bitvec_ensure(bitvec_t *v, unsigned int need)
{
  if (need > v->length)
  {
    unsigned int  length;
    bitvec_T     *vec;

    length = need;

    vec = realloc(v->vec, length * sizeof(*v->vec));
    if (vec == NULL)
      return result_OOM;

    /* wipe the new segment */
    memset(vec + v->length, 0, (length - v->length) * sizeof(*v->vec));

    v->length = length;
    v->vec    = vec;
  }

  return result_OK;
}
