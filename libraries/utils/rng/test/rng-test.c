/* utils/rng/test/rng-test.c -- tests for the stateless hash and noise */

#include <stdio.h>
#include <stdlib.h>

#include "base/utils.h"
#include "utils/rng.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

result_t rng_test(const char *resources)
{
  int s;
  int v;
  int prev;

  NOT_USED(resources);

  if (rng_hash32(1, 2) != rng_hash32(1, 2) ||
      rng_hash32(1, 2) == rng_hash32(1, 3) ||
      rng_hash32(1, 2) == rng_hash32(2, 2))
  {
    printf("rng_hash32: not a stable, keyed hash\n");
    return result_TEST_FAILED;
  }

  /* noise hits each knot exactly, stays in range and moves smoothly: the
   * steepest smoothstep step is 1.5 / period of a knot gap of up to 511,
   * plus rounding */
  prev = rng_noise1d(7, 0, 16);
  for (s = 0; s < 16 * 64; s++)
  {
    v = rng_noise1d(7, s, 16);
    if (v < -256 || v >= 256 || abs(v - prev) > 511 * 3 / (2 * 16) + 2 ||
        (s % 16 == 0 && v != (int) (rng_hash32(7, s / 16) & 511) - 256))
    {
      printf("rng_noise1d: bad value %d at step %d\n", v, s);
      return result_TEST_FAILED;
    }
    prev = v;
  }

  return result_TEST_PASSED;
}
