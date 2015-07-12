/* impl.h -- bit vectors */

#ifndef DATASTRUCT_BITVEC_IMPL_H
#define DATASTRUCT_BITVEC_IMPL_H

#define LOG2BITSPERWORD 5
#define BYTESPERWORD    (1 << (LOG2BITSPERWORD - 3))
#define WORDMASK        ((1 << LOG2BITSPERWORD) - 1)

struct bitvec
{
  unsigned int  length; /* length in words */
  unsigned int *vec;
};

result_t bitvec_ensure(bitvec_t *v, unsigned int need);

#endif /* DATASTRUCT_BITVEC_IMPL_H */
