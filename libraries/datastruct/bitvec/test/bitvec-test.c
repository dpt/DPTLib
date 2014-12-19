
#include <stdio.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "datastruct/bitvec.h"

#define NBITS 97

static void dumpbits(bitvec_t *v)
{
  int i;

  for (i = bitvec_length(v) - 1; i >= 0; i--)
    printf("%d", bitvec_get(v, i));
  printf("\n");
}

result_t bitvec_test(void)
{
  result_t  err;
  bitvec_t *v;
  int       i;
  bitvec_t *w;
  bitvec_t *x;

  printf("test: create\n");

  v = bitvec_create(0);
  if (!v)
    goto Failure;

  printf("test: set\n");

  for (i = 0; i < NBITS; i++)
  {
    err = bitvec_set(v, i);
    if (err)
      goto Failure;
  }

  printf("test: get\n");

  {
    int c;

    c = 0;
    for (i = 0; i < NBITS; i++)
      c += bitvec_get(v, i);

    printf("%d bits set\n", c);
  }

  printf("test: count\n");

  printf("%d bits set\n", bitvec_count(v));

  printf("test: clear\n");

  for (i = 0; i < NBITS; i++)
    bitvec_clear(v, i);

  printf("test: get\n");

  {
    int c;

    c = 0;
    for (i = 0; i < NBITS; i++)
      c += bitvec_get(v, i);

    printf("%d bits set\n", c);
  }

  printf("test: count\n");

  printf("%d bits set\n", bitvec_count(v));

  printf("test: length\n");

  printf("length=%d bits\n", bitvec_length(v));

  printf("test: next\n");

  printf("set every 11th bit\n");

  for (i = 0; i < NBITS; i += 11)
  {
    err = bitvec_set(v, i);
    if (err)
      goto Failure;
  }

  printf("walk through set bits\n");

  i = -1;
  do
  {
    printf("after %d ", i);
    i = bitvec_next(v, i);
    printf("comes %d\n", i);
  }
  while (i != -1);

  printf("test: destroy\n");

  bitvec_destroy(v);

  printf("test: eq\n");

  v = bitvec_create(0);
  if (!v)
    goto Failure;

  w = bitvec_create(0);
  if (!w)
    goto Failure;

  printf("created. equal? %d\n", bitvec_eq(v, w));

  err = bitvec_set(v, 1);
  if (err)
    goto Failure;

  err = bitvec_set(w, 1);
  if (err)
    goto Failure;

  printf("set bit 1 for v+w. equal? %d\n", bitvec_eq(v, w));

  err = bitvec_set(v, 1000);
  if (err)
    goto Failure;

  err = bitvec_set(w, 1000);
  if (err)
    goto Failure;

  printf("set bit 1000 for v+w. equal? %d\n", bitvec_eq(v, w));

  bitvec_clear(v, 1000);

  printf("clear bit 1000 for v. equal? %d\n", bitvec_eq(v, w));

  bitvec_clear(w, 1000);

  printf("clear bit 1000 for w. equal? %d\n", bitvec_eq(v, w));

  /* this tests that bitvecs with different internal lengths still compare
   * equal */

  bitvec_destroy(w);

  w = bitvec_create(0);
  if (!w)
    goto Failure;

  bitvec_clear(v, 1);

  printf("clear bit 1 for v. recreate w. equal? %d\n", bitvec_eq(v, w));

  bitvec_destroy(w);

  bitvec_destroy(v);

  printf("test: and\n");

  v = bitvec_create(0);
  if (!v)
    goto Failure;

  w = bitvec_create(0);
  if (!w)
    goto Failure;

  err = bitvec_set(v, 32);
  if (err)
    goto Failure;

  err = bitvec_set(w, 32);
  if (err)
    goto Failure;

  err = bitvec_and(v, w, &x);
  if (err)
    goto Failure;

  dumpbits(x);

  bitvec_destroy(x);
  bitvec_destroy(w);
  bitvec_destroy(v);

  printf("test: or\n");

  v = bitvec_create(0);
  if (!v)
    goto Failure;

  w = bitvec_create(0);
  if (!w)
    goto Failure;

  err = bitvec_set(v, 0);
  if (err)
    goto Failure;

  err = bitvec_set(w, 32);
  if (err)
    goto Failure;

  err = bitvec_or(v, w, &x);
  if (err)
    goto Failure;

  dumpbits(x);

  bitvec_destroy(x);
  bitvec_destroy(w);
  bitvec_destroy(v);

  return result_TEST_PASSED;


Failure:

  return result_TEST_FAILED;
}
