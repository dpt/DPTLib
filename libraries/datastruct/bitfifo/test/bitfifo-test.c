
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"
#include "datastruct/bitfifo.h"

#include "test/all-tests.h"

#define MAXSIZE 32

static const struct
{
  int          length; /* in bits */
  unsigned int after_dequeue;
}
expected[] =
{
  {  1, 0x00000001 },
  {  2, 0x00000003 },
  {  3, 0x00000007 },
  {  4, 0x0000000F },
  { 31, 0x7FFFFFFF },
  { 32, 0xFFFFFFFF },
};

/* shows changes from previous stats */
static void dump(const bitfifo_t *fifo)
{
  static size_t previous_used  = (size_t) -1; /* aka SIZE_MAX */
  static int    previous_full  = INT_MAX;
  static int    previous_empty = INT_MAX;

  size_t used;
  int    full;
  int    empty;

  used  = bitfifo_used(fifo);
  full  = bitfifo_full(fifo);
  empty = bitfifo_empty(fifo);

  printf("{");
  if (used != previous_used)
    printf("used=%zu ", used);
  if (full != previous_full)
    printf("full=%d ", full);
  if (empty != previous_empty)
    printf("empty=%d ", empty);
  printf("}\n");

  previous_used  = used;
  previous_full  = full;
  previous_empty = empty;
}

result_t bitfifo_test(const char *resources)
{
  const unsigned int all_ones = ~0;

  result_t     err;
  bitfifo_t   *fifo;
  int          i;
  unsigned int outbits;

  NOT_USED(resources);

  fifo = bitfifo_create(MAXSIZE);
  if (fifo == NULL)
    return result_OOM;
  dump(fifo);

  printf("test: enqueue/dequeue...\n");

  for (i = 0; i < NELEMS(expected); i++)
  {
    int length;

    length = expected[i].length;

    printf("%d bits\n", length);

    /* put 'size_to_try' 1 bits into the queue */
    err = bitfifo_enqueue(fifo, &all_ones, 0, length);
    if (err)
      goto Failure;
    dump(fifo);

    /* pull the bits back out */
    outbits = 0;
    err = bitfifo_dequeue(fifo, &outbits, length);
    if (err)
      goto Failure;
    dump(fifo);

    if (outbits != expected[i].after_dequeue)
      printf("*** difference: %.8x <> %.8x\n",
             outbits,
             expected[i].after_dequeue);
  }

  printf("...done\n");

  printf("test: completely fill up the fifo\n");
  err = bitfifo_enqueue(fifo, &all_ones, 0, MAXSIZE);
  if (err)
    goto Failure;
  dump(fifo);

  printf("test: enqueue another bit (should error)\n");
  err = bitfifo_enqueue(fifo, &all_ones, 0, 1);
  if (err != result_BITFIFO_FULL)
    goto Failure;
  dump(fifo);

  printf("test: do 32 dequeue-enqueue ops...\n");
  for (i = 0; i < MAXSIZE; i++)
  {
    printf("dequeue a single bit\n");
    err = bitfifo_dequeue(fifo, &outbits, 1);
    if (err)
      goto Failure;
    dump(fifo);

    printf("enqueue a single bit\n");
    err = bitfifo_enqueue(fifo, &all_ones, 0, 1);
    if (err)
      goto Failure;
    dump(fifo);
  }

  printf("...done\n");

  bitfifo_destroy(fifo);

  return result_TEST_PASSED;


Failure:

  return result_TEST_FAILED;
}
