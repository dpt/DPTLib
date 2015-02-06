
#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "utils/array.h"

#include "utils/bsearch.h"

result_t bsearch_test(void); /* suppress "No previous prototype" warning */

result_t bsearch_test(void)
{
  int i;
  int nfailures = 0;

  static const int ints[] = { INT_MIN, INT_MIN + 1, INT_MAX - 1, INT_MAX };

  printf("test: int array\n");

  for (i = 0; i < NELEMS(ints); i++)
  {
    printf("searching for %d...", ints[i]);

    if (bsearch_int(ints, NELEMS(ints), sizeof(ints[0]), ints[i]) == -1)
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

    if (bsearch_uint(uints, NELEMS(uints), sizeof(uints[0]), uints[i]) == -1)
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

    if (bsearch_short(shorts, NELEMS(shorts), sizeof(shorts[0]), shorts[i]) == -1)
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

    if (bsearch_ushort(ushorts, NELEMS(ushorts), sizeof(ushorts[0]), ushorts[i]) == -1)
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


  if (nfailures > 0)
    goto Failure;
  
  return result_TEST_PASSED;
  
  
Failure:
  
  return result_TEST_FAILED;
}
