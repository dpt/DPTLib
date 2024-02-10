
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"

#include "io/stream.h"
#include "io/stream-stdio.h"
#include "io/stream-mtfcomp.h"
#include "io/stream-packbits.h"
#include "io/stream-mem.h"

#include "test/all-tests.h"

#define BUFSZ 0   /* use the default buffer size in each case */
#define USEFILE 0 /* some of tests will use files rather than in-core */

static result_t test_getc_empty(void)
{
  static const unsigned char block[1] = { 'X' };

  result_t  rc = result_TEST_PASSED;
  stream_t *s;
  int       c;

  printf("Test - Empty\n");

  rc = stream_mem_create(block, 0, &s);
  if (rc)
    goto Failure;

  if (stream_remaining(s) != 0)
    return result_TEST_FAILED;

  c = stream_getc(s);
  if (c != stream_EOF)
    return result_TEST_FAILED;

  stream_destroy(s);

  return rc;


Failure:
  return rc;
}

static result_t test_getc_1byte(void)
{
  static const unsigned char block[1] = { 'X' };

  result_t  rc = result_TEST_PASSED;
  stream_t *s;
  int       c;

  printf("Test - Single Byte\n");

  rc = stream_mem_create(block, sizeof(block), &s);
  if (rc)
    goto Failure;

  if (stream_remaining(s) > 1)
    goto Failure;

  c = stream_getc(s);
  if (c != 'X')
    goto Failure;

  if (stream_remaining(s) != 0)
    goto Failure;

  c = stream_getc(s);
  if (c != stream_EOF)
    goto Failure;

  stream_destroy(s);

  return rc;


Failure:
  return result_TEST_FAILED;
}

typedef enum Index
{
  Source,
  MTFComp,
  MTFDecomp,
  PackBitsComp,
  PackBitsDecomp,
  MaxStreams,
}
Index_t;

#if !USEFILE
static const unsigned char input[] =
"abbcccddddeeeeeffffff\n"
"a bb ccc dddd eeeee ffffff\n"
"We all love aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaardvarks.\n"
"and iiiiii\n"
"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n";
#endif

static result_t test_compressors(void)
{
  result_t             rc = result_TEST_PASSED;
#if USEFILE
  FILE                *in;
  FILE                *out;
#endif
  stream_t            *stream[MaxStreams];
  int                  c;
  int                  i;
#if !USEFILE
  const unsigned char *p;
#endif

  printf("Test - Compressors\n");

#if USEFILE
  in = fopen("stream-test-input", "rb");
  if (in == NULL)
    goto Failure;

  out = fopen("stream-test-output", "wb");
  if (out == NULL)
    goto Failure;
#endif

#if USEFILE
  err = stream_stdio_create(in, BUFSZ, &stream[Source]);
#else
  rc = stream_mem_create(input, NELEMS(input), &stream[Source]);
#endif
  if (rc)
    goto Failure;

  /* source -> mtfcomp -> mtfdecomp -> packbitscomp -> packbitsdecomp */

  rc = stream_mtfcomp_create(stream[Source], BUFSZ, &stream[MTFComp]);
  if (rc)
    goto Failure;

  rc = stream_mtfdecomp_create(stream[MTFComp], BUFSZ, &stream[MTFDecomp]);
  if (rc)
    goto Failure;

  rc = stream_packbitscomp_create(stream[MTFDecomp], BUFSZ, &stream[PackBitsComp]);
  if (rc)
    goto Failure;

  rc = stream_packbitsdecomp_create(stream[PackBitsComp], BUFSZ, &stream[PackBitsDecomp]);
  if (rc)
    goto Failure;

#if USEFILE
  for (;;)
  {
    c = stream_getc(stream[PackBitsDecomp]);
    if (c == EOF)
      break;

    fputc(c, out);
  }
#else
  for (p = input; ; p++)
  {
    c = stream_getc(stream[PackBitsDecomp]);
    if (c == EOF)
      break;

    if (c != *p)
      printf("difference at %ld\n", p - input);
  }

  printf("%ld bytes processed.\n", p - input);
#endif

  for (i = 0; i < MaxStreams; i++)
    stream_destroy(stream[i]);

#if USEFILE
  fclose(out);
  /* fclose(in) is handled by the FILE stream's destruction */
#endif

  return rc;


Failure:
  return result_TEST_FAILED;
}

static result_t test_block(void)
{
  result_t             rc = result_TEST_PASSED;
#if USEFILE
  FILE                *in;
  FILE                *out;
#endif
  stream_t            *s;
  int                  c;
#if !USEFILE
  const unsigned char *p;
#endif

  printf("Test - Block\n");

#if USEFILE
  in = fopen("stream-test-input", "rb");
  if (in == NULL)
    goto Failure;

  out = fopen("stream-test-output", "wb");
  if (out == NULL)
    goto Failure;
#endif

#if USEFILE
  err = stream_stdio_create(in, BUFSZ, &s);
#else
  rc = stream_mem_create(input, NELEMS(input), &s);
#endif
  if (rc)
    goto Failure;

#if USEFILE
  for (;;)
  {
    //c = stream_getc(s);
    //if (c == EOF)
    //  break;

    fputc(c, out);
  }
#else
  for (p = input; ; p++)
  {
    const stream_size_t need = 100;
    stream_size_t       remaining;

    remaining = stream_remaining_and_fill(s);
    if (remaining < need)
    {
      printf("needed %zu bytes, but only %zu bytes in buffer\n", need, remaining);
      break;
    }

    c = stream_getc(s);
    if (c == EOF)
      break;

    if (c != *p)
    {
      printf("difference at %ld\n", p - input);
      rc = result_TEST_FAILED;
    }
  }

  printf("%ld bytes processed.\n", p - input);
#endif

  stream_destroy(s);

#if USEFILE
  fclose(out);
  /* fclose(in) is handled by the FILE stream's destruction */
#endif

  return rc;


Failure:
  return result_TEST_FAILED;
}

result_t stream_test(const char *resources)
{
  result_t rc;

  NOT_USED(resources);

  rc = test_getc_empty();
  if (rc)
    goto Failure;

  rc = test_getc_1byte();
  if (rc)
    goto Failure;

  rc = test_compressors();
  if (rc)
    goto Failure;

  rc = test_block();
  if (rc)
    goto Failure;

  return result_TEST_PASSED;


Failure:
  printf("\n\n*** result=%x\n", rc);
  return result_TEST_FAILED;
}
