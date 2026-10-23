/* utils/bsearch/test/bsearch-test.c */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"

#include "utils/bsearch.h"

#include "test/all-tests.h"

result_t bsearch_test(const char *resources)
{
  static const int ints[] = { INT_MIN, INT_MIN + 1, INT_MAX - 1, INT_MAX };

  int i;
  int nfailures = 0;

  NOT_USED(resources);

  printf("test: int array\n");

  for (i = 0; i < NELEMS(ints); i++)
  {
    printf("searching for %d...", ints[i]);

    if (bsearch_int(ints, NELEMS(ints), sizeof(ints[0]), ints[i]) != i)
    {
      printf("failed!\n");
      nfailures++;
    }
    else
    {
      printf("ok!\n");
    }
  }

  printf("\n");


  static const unsigned int uints[] = { 0, 1, UINT_MAX - 1, UINT_MAX };

  printf("test: unsigned int array\n");

  for (i = 0; i < NELEMS(uints); i++)
  {
    printf("searching for %u...", uints[i]);

    if (bsearch_uint(uints, NELEMS(uints), sizeof(uints[0]), uints[i]) != i)
    {
      printf("failed!\n");
      nfailures++;
    }
    else
    {
      printf("ok!\n");
    }
  }

  printf("\n");


  static const short shorts[] = { SHRT_MIN, SHRT_MIN + 1, SHRT_MAX - 1, SHRT_MAX };

  printf("test: short array\n");

  for (i = 0; i < NELEMS(shorts); i++)
  {
    printf("searching for %d...", shorts[i]);

    if (bsearch_short(shorts, NELEMS(shorts), sizeof(shorts[0]), shorts[i]) != i)
    {
      printf("failed!\n");
      nfailures++;
    }
    else
    {
      printf("ok!\n");
    }
  }

  printf("\n");


  static const unsigned short ushorts[] = { 0, 1, USHRT_MAX - 1, USHRT_MAX };

  printf("test: unsigned short array\n");

  for (i = 0; i < NELEMS(ushorts); i++)
  {
    printf("searching for %u...", ushorts[i]);

    if (bsearch_ushort(ushorts, NELEMS(ushorts), sizeof(ushorts[0]), ushorts[i]) != i)
    {
      printf("failed!\n");
      nfailures++;
    }
    else
    {
      printf("ok!\n");
    }
  }

  printf("\n");

  printf("test: values absent from each array\n");

  if (bsearch_int(ints, NELEMS(ints), sizeof(ints[0]), 0) != -1 ||
      bsearch_uint(uints, NELEMS(uints), sizeof(uints[0]), 2) != -1 ||
      bsearch_short(shorts, NELEMS(shorts), sizeof(shorts[0]), 0) != -1 ||
      bsearch_ushort(ushorts, NELEMS(ushorts), sizeof(ushorts[0]), 2) != -1)
  {
    printf("found a value that isn't there!\n");
    nfailures++;
  }

  printf("\n");


  printf("test: every array length up to 17, every value hit and missed\n");

  {
    int evens[17];
    int n;
    int want;
    int expect;

    for (i = 0; i < NELEMS(evens); i++)
      evens[i] = i * 2;

    /* 0..17 covers the empty array, power-of-two lengths and the
     * rangeShift path taken by lengths that aren't. Odd values fall between
     * (or beyond) elements so must miss. */
    for (n = 0; n <= NELEMS(evens); n++)
    {
      for (want = -1; want <= n * 2; want++)
      {
        expect = (want >= 0 && want < n * 2 && (want & 1) == 0) ? want / 2 : -1;
        if (bsearch_int(evens, n, sizeof(evens[0]), want) != expect)
        {
          printf("length %d, searching for %d: expected %d\n", n, want, expect);
          nfailures++;
        }
      }
    }
  }

  printf("\n");


  printf("test: stride steps over interleaved fields\n");

  {
    static const struct
    {
      int key;
      int value;
    }
    pairs[] = { { 10, 100 }, { 20, 200 }, { 30, 300 }, { 40, 400 }, { 50, 500 } };

    for (i = 0; i < NELEMS(pairs); i++)
    {
      /* searching for a value field must miss: only keys are visited */
      if (bsearch_int(&pairs[0].key, NELEMS(pairs), sizeof(pairs[0]), pairs[i].key) != i ||
          bsearch_int(&pairs[0].key, NELEMS(pairs), sizeof(pairs[0]), pairs[i].value) != -1)
      {
        printf("stride search for key %d failed!\n", pairs[i].key);
        nfailures++;
      }
    }
  }

  printf("\n");

  printf("bsearch: %d failure(s)\n", nfailures);

  return (nfailures > 0) ? result_TEST_FAILED : result_TEST_PASSED;
}
