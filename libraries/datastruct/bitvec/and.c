/* and.c -- bit vectors */

#include <stdlib.h>

#include "base/utils.h"

#include "datastruct/bitvec.h"

#include "impl.h"

result_t bitvec_and(const bitvec_t *a, const bitvec_t *b, bitvec_t **c)
{
  int       l;
  bitvec_t *v;
  int       i;

  *c = NULL;

  /* all set bits will be contained in the minimum span of both arrays */
  l = MIN(a->length, b->length);

  /* create vec, ensuring that enough space is allocated to perform the op */
  v = bitvec_create(l << LOG2BITSPERWORD);
  if (v == NULL)
    return result_OOM;

  for (i = 0; i < l; i++)
    v->vec[i] = a->vec[i] & b->vec[i];

  *c = v;

  return result_OK;
}
