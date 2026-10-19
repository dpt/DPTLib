/* utils/barith/test/barith-test.c -- tests for binary arithmetic */

#include <stdint.h>
#include <stdio.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"

#include "utils/barith.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

/* Checks the 64-bit functions for x, and the 32-bit ones too when x fits,
 * against answers worked out a bit at a time. Returns non-zero on success. */
static int barith_check(uint64_t x)
{
  int      hi;
  int      lo;
  int      bits;
  int      i;
  uint64_t rev;
  uint64_t pow2gt;
  uint32_t rev32;
  uint32_t x32;

  hi   = -1;
  lo   = -1;
  bits = 0;
  rev  = 0;
  for (i = 0; i < 64; i++)
  {
    if ((x >> i) & 1)
    {
      if (lo < 0)
        lo = i;
      hi = i;
      bits++;
      rev |= 1ull << (63 - i);
    }
  }

  pow2gt = (hi == 63) ? 0 : 1ull << (hi + 1);

  if (countbits_64(x) != bits ||
      clz_64(x) != 63 - hi ||
      ctz_64(x) != lo ||
      msb_64(x) != 1ull << hi ||
      power2le_64(x) != 1ull << hi ||
      power2gt_64(x) != pow2gt ||
      floorlog2_64(x) != (uint64_t) hi ||
      ceillog2_64(x) != (uint64_t) (hi + (bits > 1)) ||
      reversebits_64(x) != rev ||
      LSB(x) != 1ull << lo ||
      ISPOWER2(x) != (bits == 1))
  {
    printf("Failure: 64-bit function wrong for %llx\n",
           (unsigned long long) x);
    return 0;
  }

  if (x > UINT32_MAX)
    return 1;

  x32   = (uint32_t) x;
  rev32 = (uint32_t) (rev >> 32);

  if (countbits_32(x32) != bits ||
      clz_32(x32) != 31 - hi ||
      ctz_32(x32) != lo ||
      msb_32(x32) != 1u << hi ||
      power2le_32(x32) != 1u << hi ||
      power2gt_32(x32) != (uint32_t) pow2gt ||
      floorlog2_32(x32) != (uint32_t) hi ||
      ceillog2_32(x32) != (uint32_t) (hi + (bits > 1)) ||
      reversebits_32(x32) != rev32)
  {
    printf("Failure: 32-bit function wrong for %x\n", x32);
    return 0;
  }

  if (x32 < 256 && clz8(x32) != 7 - hi)
  {
    printf("Failure: clz8 wrong for %x\n", x32);
    return 0;
  }

  return 1;
}

result_t barith_test(const char *resources)
{
  static const uint64_t mixed[] =
  {
    0x12345678u, 0xdeadbeefu, 0xffffffffu, 0x80000001u,
    0x0123456789abcdefull, 0xfedcba9876543210ull, UINT64_MAX
  };

  int      i;
  uint64_t bit;

  NOT_USED(resources);

  printf("test: bit counts, logs, powers of two and reversal\n");

  /* each single bit, and the neighbours either side of it */
  for (i = 0; i < 64; i++)
  {
    bit = 1ull << i;
    if (!barith_check(bit) ||
        !barith_check(bit | 1) ||
        (i > 1 && !barith_check(bit - 1)) ||
        (i < 63 && !barith_check(bit | (bit << 1))))
      return result_TEST_FAILED;
  }

  for (i = 0; i < (int) NELEMS(mixed); i++)
    if (!barith_check(mixed[i]))
      return result_TEST_FAILED;

  printf("test: zero and zero-means-2^N\n");

  if (msb_32(0) != 0 || msb_64(0) != 0 ||
      reversebits_32(0) != 0 || reversebits_64(0) != 0 ||
      clz8(0) != 8 || countbits_32(0) != 0 || countbits_64(0) != 0)
  {
    printf("Failure: zero argument\n");
    return result_TEST_FAILED;
  }

  if (ZEROIS2N(0, 3) != 8 || ZEROIS2N(1, 3) != 1 || ZEROIS2N(7, 3) != 7 ||
      ZEROIS2N(0, 1) != 2)
  {
    printf("Failure: ZEROIS2N\n");
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}
