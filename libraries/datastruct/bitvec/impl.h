/* impl.h -- bit vectors */

#ifndef DATASTRUCT_BITVEC_IMPL_H
#define DATASTRUCT_BITVEC_IMPL_H

#ifdef BITVEC_64
#define bitvec_T          unsigned long long
#define bitvec_1          1ull
#define bitvec_ctz        ctz_64
#define bitvec_countbits  countbits_64
#define LOG2BITSPERWORD   6
#else
#define bitvec_T          unsigned int
#define bitvec_1          1u
#define bitvec_ctz        ctz
#define bitvec_countbits  countbits
#define LOG2BITSPERWORD   5
#endif

#define BYTESPERWORD    (1 << (LOG2BITSPERWORD - 3))
#define WORDMASK        ((1 << LOG2BITSPERWORD) - 1)

struct bitvec
{
  unsigned int  length; /* length in words */
  bitvec_T     *vec;
};

result_t bitvec_ensure(bitvec_t *v, unsigned int need);

#endif /* DATASTRUCT_BITVEC_IMPL_H */
