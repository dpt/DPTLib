/* --------------------------------------------------------------------------
 *    Name: impl.h
 * Purpose: Bit vectors
 * ----------------------------------------------------------------------- */

#ifndef DATASTRUCT_BITVEC_IMPL_H
#define DATASTRUCT_BITVEC_IMPL_H

struct bitvec_t
{
  unsigned int  length; /* length in words */
  unsigned int *vec;
};

result_t bitvec_ensure(bitvec_t *v, int need);

#endif /* DATASTRUCT_BITVEC_IMPL_H */
