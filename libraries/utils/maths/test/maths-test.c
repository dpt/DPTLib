/* utils/maths/test/maths-test.c */

#include <stdio.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"

#include "utils/maths.h"

#include "test/all-tests.h"

result_t maths_test(const char *resources)
{
  /* divisors from the pixel loops which use div_recip, plus the extremes */
  static const unsigned int divisors[] =
  {
    1, 2, 3, 7, 100, 255, 256, 257, 1600, 4800, 65025, 65535, 0x7FFFFFFFu
  };
  /* numerators near the top of the range */
  static const unsigned int bigs[] =
  {
    0x7FFFFFFFu, 0x7FFFFFFEu, 0x7FFF0001u, 0x40000000u, 0x3FFFFFFFu
  };

  unsigned int d;
  unsigned int r;
  unsigned int n;
  int          i;
  int          j;
  int          nfailures = 0;

  NOT_USED(resources);

  printf("test: gcd\n");

  if (gcd(12, 18) != 6 || gcd(17, 5) != 1)
  {
    printf("gcd failed!\n");
    nfailures++;
  }

  printf("test: div_recip, every alpha divisor and small numerator\n");

  for (d = 1; d < 256; d++)
  {
    r = DIV_RECIP(d);
    for (n = 0; n < 1u << 17; n++)
      if (div_recip(n, d, r) != n / d)
      {
        printf("%u / %u failed!\n", n, d);
        nfailures++;
        break;
      }
  }

  printf("test: div_recip, assorted divisors and large numerators\n");

  for (i = 0; i < NELEMS(divisors); i++)
  {
    d = divisors[i];
    r = DIV_RECIP(d);
    for (j = 0; j < NELEMS(bigs); j++)
    {
      n = bigs[j];
      if (div_recip(n, d, r) != n / d ||
          div_recip(n - n % d, d, r) != n / d ||
          (n % d != 0 && n >= d && div_recip(n - n % d - 1, d, r) != n / d - 1))
      {
        printf("%u / %u failed!\n", n, d);
        nfailures++;
      }
    }
  }

  printf("maths: %d failure(s)\n", nfailures);

  return (nfailures > 0) ? result_TEST_FAILED : result_TEST_PASSED;
}
