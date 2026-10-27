/* utils/array/test/array-test.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"

#include "utils/array.h"

#include "test/all-tests.h"

/* ----------------------------------------------------------------------- */

static int nreallocs;

static void *counting_malloc(size_t size)
{
  return malloc(size);
}

static void *counting_realloc(void *ptr, size_t size)
{
  nreallocs++;
  return realloc(ptr, size);
}

static void counting_free(void *ptr)
{
  free(ptr);
}

static const alloc_t counting_alloc =
{
  counting_malloc, counting_realloc, counting_free
};

static void *failing_malloc(size_t size)
{
  NOT_USED(size);
  return NULL;
}

static void *failing_realloc(void *ptr, size_t size)
{
  NOT_USED(ptr);
  NOT_USED(size);
  return NULL;
}

static const alloc_t failing_alloc =
{
  failing_malloc, failing_realloc, counting_free
};

/* ----------------------------------------------------------------------- */

result_t array_test(const char *resources)
{
  int delelem1[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  int delelem2[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

  union
  {
    int  i[9];
    char c[9 * sizeof(int)];
  }
  sqz = { { 1, 2, 3, 4, 5, 6, 7, 8, 9 } };

  union
  {
    char c[9 * sizeof(int)];
    int  i[9];
  }
  str = { { 1, 2, 3, 4, 5, 6, 7, 8, 9 } };

  int   t;
  int   i;

  int  *arr;
  int   allocated;
  int   used;
  int   rc;

  int  *arr2;
  int   allocated2;

  char *dup;

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

  array_squeeze((unsigned char *) &sqz.i, 9, sizeof(int), sizeof(char));

  t = 0;
  for (i = 0; i < 9; i++)
    t += sqz.c[i];

  if (t != 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9)
  {
    printf("unexpected checksum %d\n", t);
    goto Failure;
  }


  printf("test: stretch elements\n");

  array_stretch((unsigned char *) &str.c, 9, sizeof(char), sizeof(int), 0);

  t = 0;
  for (i = 0; i < 9; i++)
    t += str.i[i];

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


  printf("test: grow array through allocator hooks\n");

  arr       = NULL;
  allocated = 0;
  nreallocs = 0;

  rc = array_grow_alloc(&counting_alloc, (void **) &arr, sizeof(*arr), 0,
                        &allocated, 3, 1);
  if (rc != 0 || arr == NULL || allocated != 4 || nreallocs != 1)
  {
    printf("unexpected result rc=%d allocated=%d nreallocs=%d\n",
           rc, allocated, nreallocs);
    free(arr);
    goto Failure;
  }

  rc = array_grow_alloc(&failing_alloc, (void **) &arr, sizeof(*arr), 4,
                        &allocated, 1, 1);
  if (rc != 1 || arr == NULL || allocated != 4)
  {
    printf("unexpected OOM result rc=%d allocated=%d\n", rc, allocated);
    free(arr);
    goto Failure;
  }

  free(arr);


  printf("test: duplicate string through allocator hooks\n");

  dup = alloc_strdup(&alloc_default, "hello");
  if (dup == NULL || strcmp(dup, "hello") != 0)
  {
    printf("unexpected strdup result\n");
    free(dup);
    goto Failure;
  }

  free(dup);

  if (alloc_strdup(&alloc_default, NULL) != NULL ||
      alloc_strdup(&failing_alloc, "x") != NULL)
  {
    printf("unexpected strdup of NULL or under OOM\n");
    goto Failure;
  }


  return result_TEST_PASSED;


Failure:

  return result_TEST_FAILED;
}
