/* utils/bytesex/test/bytesex-test.c */

#include <stdio.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"

#include "utils/bytesex.h"

#include "test/all-tests.h"

result_t bytesex_test(const char *resources)
{
  static const unsigned char mem[4] = { 0x12, 0x34, 0x56, 0x78 };

  int                        nfailures = 0;
  unsigned int               longs[3]  = { 0x12345678U, 0xdeadbeefU, 0 };
  int                        i;
  union
  {
    unsigned int       align; /* forces 4-byte alignment of shorts[] */
    unsigned short int shorts[6];
  }
  u = { 0 };

  NOT_USED(resources);

  printf("test: scalars\n");

  if (rev_s(0x1234) != 0x3412)                   nfailures++;
  if (rev_s_m(mem) != 0x1234)                    nfailures++;
  if (rev_s_pair(0x12345678U) != 0x34127856U)    nfailures++;
  if (rev_s_pair_m(mem) != 0x56781234U)          nfailures++;
  if (rev_l(0x12345678U) != 0x78563412U)         nfailures++;
  if (rev_l_m(mem) != 0x12345678U)               nfailures++;

  printf("test: long block\n");

  rev_l_block(longs, NELEMS(longs));

  if (longs[0] != 0x78563412U ||
      longs[1] != 0xefbeaddeU ||
      longs[2] != 0)
    nfailures++;

  printf("test: short block (unaligned start, odd trailer)\n");

  for (i = 0; i < 6; i++)
    u.shorts[i] = (unsigned short int)(0x0100 * (i + 1) + i);

  /* skip shorts[0] so the block starts misaligned; leave shorts[5] alone */
  rev_s_block(&u.shorts[1], 4);

  if (u.shorts[0] != 0x0100)
    nfailures++;
  for (i = 1; i < 5; i++)
    if (u.shorts[i] != rev_s((unsigned short int)(0x0100 * (i + 1) + i)))
      nfailures++;
  if (u.shorts[5] != 0x0605)
    nfailures++;

  printf("test: short block (aligned start, odd count)\n");

  rev_s_block(&u.shorts[0], 3);

  if (u.shorts[0] != 0x0001 ||
      u.shorts[1] != 0x0201 ||
      u.shorts[2] != 0x0302 ||
      u.shorts[3] != 0x0304) /* untouched */
    nfailures++;

  printf("test: short block (empty)\n");

  rev_s_block(&u.shorts[0], 0);

  if (u.shorts[0] != 0x0001)
    nfailures++;

  if (nfailures > 0)
    printf("%d failures\n", nfailures);

  return nfailures == 0 ? result_TEST_PASSED : result_TEST_FAILED;
}
