
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

  int  *arr;
  int   allocated;
  int   used;
  int   rc;

  int  *arr2;
  int   allocated2;

  NOT_USED(resources);


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


  printf("test: grow array (initial allocation)\n");

  arr       = NULL;
  allocated = 0;
  used      = 0;

  rc = array_grow((void **) &arr, sizeof(*arr), used, &allocated, 5, 4);
  if (rc != 0 || arr == NULL || allocated != 8)
  {
    printf("unexpected result rc=%d allocated=%d\n", rc, allocated);
    goto Failure;
  }

  for (i = 0; i < 5; i++)
    arr[i] = i + 1;
  used = 5;


  printf("test: grow array (existing capacity is sufficient)\n");

  rc = array_grow((void **) &arr, sizeof(*arr), used, &allocated, 2, 1);
  if (rc != 0 || allocated != 8)
  {
    printf("unexpected reallocation rc=%d allocated=%d\n", rc, allocated);
    free(arr);
    goto Failure;
  }

  t = 0;
  for (i = 0; i < 5; i++)
    t += arr[i];

  if (t != 1 + 2 + 3 + 4 + 5)
  {
    printf("unexpected checksum %d\n", t);
    free(arr);
    goto Failure;
  }


  printf("test: grow array (forces reallocation)\n");

  used = allocated; /* pretend the array is full */

  rc = array_grow((void **) &arr, sizeof(*arr), used, &allocated, 1, 1);
  if (rc != 0 || allocated != 16)
  {
    printf("unexpected result rc=%d allocated=%d\n", rc, allocated);
    free(arr);
    goto Failure;
  }

  t = 0;
  for (i = 0; i < 5; i++)
    t += arr[i];

  if (t != 1 + 2 + 3 + 4 + 5)
  {
    printf("unexpected checksum %d after grow\n", t);
    free(arr);
    goto Failure;
  }


  printf("test: shrink array\n");

  rc = array_shrink((void **) &arr, sizeof(*arr), 5, &allocated);
  if (rc != 0 || allocated != 5)
  {
    printf("unexpected result rc=%d allocated=%d\n", rc, allocated);
    free(arr);
    goto Failure;
  }

  t = 0;
  for (i = 0; i < 5; i++)
    t += arr[i];

  if (t != 1 + 2 + 3 + 4 + 5)
  {
    printf("unexpected checksum %d after shrink\n", t);
    free(arr);
    goto Failure;
  }

  free(arr);


  printf("test: grow array (minimum forces a larger allocation)\n");

  arr2       = NULL;
  allocated2 = 0;

  rc = array_grow((void **) &arr2, sizeof(*arr2), 0, &allocated2, 1, 10);
  if (rc != 0 || allocated2 != 16)
  {
    printf("unexpected result rc=%d allocated2=%d\n", rc, allocated2);
    free(arr2);
    goto Failure;
  }

  free(arr2);


  return result_TEST_PASSED;


Failure:

  return result_TEST_FAILED;
}
