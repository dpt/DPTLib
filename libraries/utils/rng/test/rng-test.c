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
  int y;
  int x;

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

  /* 2-D noise on a 64-pixel tile of 8 x 4 cells: in range, hits each knot
   * exactly at a cell corner and repeats every tile */
  for (y = 0; y < 64; y++)
    for (x = 0; x < 64; x++)
    {
      v = rng_noise2d(7, x, y, 6, 3, 2);
      if (v < 0 || v >= 256 ||
          v != rng_noise2d(7, x + 64, y + 128, 6, 3, 2) ||
          (x % 8 == 0 && y % 16 == 0 &&
           v != (int) (rng_hash32(7, (y / 16) << 3 | x / 8) & 255)))
      {
        printf("rng_noise2d: bad value %d at %d,%d\n", v, x, y);
        return result_TEST_FAILED;
      }
    }

  return result_TEST_PASSED;
}
