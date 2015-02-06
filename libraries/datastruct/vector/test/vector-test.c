
#include <stdio.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "utils/array.h"
#include "base/result.h"
#include "datastruct/vector.h"

#define WIDTH 4 /* bytes */

static const int set_lengths[] = { 25, 50, 75, 100 };

static const int set_widths[] = { 3, 2, 1, 2, 3, 4 };

static const int primes[] =
{
   2,   3,   5,   7,  11,  13,  17, 19,
  23,  29,  31,  37,  41,  43,  47, 53,
  59,  61,  67,  71,  73,  79,  83, 89,
  97, 101, 103, 107, 109, 113, 127
};

int vector_test(void)
{
  vector_t *v;
  int       i;
  int      *p;

  printf("test: create\n");

  v = vector_create(WIDTH);
  if (v == NULL)
    goto Failure;


  printf("test: clear\n");

  vector_clear(v);


  printf("test: length\n");

  printf("length = %zu\n", vector_length(v));


  printf("test: set length\n");

  for (i = 0; i < NELEMS(set_lengths); i++)
  {
    int new_length;

    printf("test: set length to %d\n", set_lengths[i]);

    vector_set_length(v, set_lengths[i]);

    new_length = vector_length(v);

    printf("new length = %d\n", new_length);

    if (new_length != set_lengths[i])
    {
      printf("*** did not resize as expected!\n");
      goto Failure;
    }
  }


  printf("test: get / populate with values\n");

  p = vector_get(v, 0);
  for (i = 0; i < NELEMS(primes); i++)
    *p++ = primes[i];

  printf("...populated\n");


  printf("test: set width\n");

  for (i = 0; i < NELEMS(set_widths); i++)
  {
    int new_width;

    printf("test: set width to %d\n", set_widths[i]);

    vector_set_width(v, set_widths[i]);

    new_width = vector_width(v);

    printf("new width = %d\n", new_width);

    if (new_width != set_widths[i])
    {
      printf("*** did not resize as expected!\n");
      goto Failure;
    }

    printf("...resized\n");
  }


  printf("test: ensure values are intact\n");

  p = vector_get(v, 0);
  for (i = 0; i < NELEMS(primes); i++)
  {
    if (*p != primes[i])
    {
      printf("*** %d ought to be %d at index %d\n", *p, primes[i], i);
      goto Failure;
    }

    p++;
  }

  printf("...yes, they are\n");



  printf("test: destroy\n");

  vector_destroy(v);


  return result_TEST_PASSED;


Failure:

  return result_TEST_FAILED;
}
