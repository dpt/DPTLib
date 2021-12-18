
#include <stdio.h>
#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"

#include "utils/array.h"

#include "test/all-tests.h"

result_t array_test(const char *resources)
{
  int delelem1[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  int delelem2[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

  union
  {
    int  i[9];
    char c[9 * sizeof(int)];
  }
  sqz1 = { { 1, 2, 3, 4, 5, 6, 7, 8, 9 } };

  union
  {
    int  i[9];
    char c[9 * sizeof(int)];
  }
  sqz2 = { { 1, 2, 3, 4, 5, 6, 7, 8, 9 } };

  union
  {
    char c[9 * sizeof(int)];
    int  i[9];
  }
  str1 = { { 1, 2, 3, 4, 5, 6, 7, 8, 9 } };

  union
  {
    char c[9 * sizeof(int)];
    int  i[9];
  }
  str2 = { { 1, 2, 3, 4, 5, 6, 7, 8, 9 } };

  int t;
  int i;


  printf("test: delete element\n");

  array_delete_element(delelem1,
                       sizeof(delelem1[0]),
                       NELEMS(delelem1),
                       4);

  t = 0;
  for (i = 0; i < 9 - 1; i++)
    t += delelem1[i];

  if (t != 1 + 2 + 3 + 4 + 6 + 7 + 8 + 9)
  {
    printf("unexpected checksum\n");
    goto Failure;
  }


  printf("test: delete elements\n");

  array_delete_elements(delelem2,
                        sizeof(delelem2[0]),
                        NELEMS(delelem2),
                        3, 5 /* inclusive */);

  t = 0;
  for (i = 0; i < 9 - 3; i++)
    t += delelem2[i];

  if (t != 1 + 2 + 3 + 7 + 8 + 9)
  {
    printf("unexpected checksum\n");
    goto Failure;
  }


  printf("test: squeeze elements\n");

  array_squeeze1((unsigned char *) &sqz1.i, 9, sizeof(int), sizeof(char));

  t = 0;
  for (i = 0; i < 9; i++)
    t += sqz1.c[i];

  if (t != 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9)
  {
    printf("unexpected checksum %d\n", t);
    goto Failure;
  }


  printf("test: squeeze elements variant 2\n");

  array_squeeze2((unsigned char *) &sqz2.i, 9, sizeof(int), sizeof(char));

  t = 0;
  for (i = 0; i < 9; i++)
    t += sqz2.c[i];

  if (t != 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9)
  {
    printf("unexpected checksum %d\n", t);
    goto Failure;
  }


  printf("test: stretch elements\n");

  array_stretch1((unsigned char *) &str1.c, 9, sizeof(char), sizeof(int), 0);

  t = 0;
  for (i = 0; i < 9; i++)
    t += str1.i[i];

  if (t != 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9)
  {
    printf("unexpected checksum %d\n", t);
    goto Failure;
  }


  printf("test: stretch elements variant 2\n");

  array_stretch2((unsigned char *) &str2.c, 9, sizeof(char), sizeof(int), 0);

  t = 0;
  for (i = 0; i < 9; i++)
    t += str2.i[i];

  if (t != 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9)
  {
    printf("unexpected checksum %d\n", t);
    goto Failure;
  }


  return result_TEST_PASSED;


Failure:

  return result_TEST_FAILED;
}
