/* or.c -- bit vectors */

#include <stdlib.h>

#include "utils/minmax.h"

#include "datastruct/bitvec.h"

#include "impl.h"

result_t bitvec_or(const bitvec_t *a, const bitvec_t *b, bitvec_t **c)
{
  int             min, max;
  bitvec_t       *v;
  int             i;
  const bitvec_t *p;

  *c = NULL;

  min = MIN(a->length, b->length);
  max = MAX(a->length, b->length);

  /* create vec, ensuring that enough space is allocated to perform the op */
  v = bitvec_create(max << LOG2BITSPERWORD);
  if (v == NULL)
    return result_OOM;

  for (i = 0; i < min; i++)
    v->vec[i] = a->vec[i] | b->vec[i];

  p = (a->length > b->length) ? a : b;

  for (i = min; i < max; i++)
    v->vec[i] = p->vec[i];

  *c = v;

  return result_OK;
}
