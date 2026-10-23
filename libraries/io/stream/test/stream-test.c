/* io/stream/test/stream-test.c */

#include <assert.h>
#include <stddef.h>
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
    {
      printf("difference at %td\n", p - input);
      rc = result_TEST_FAILED;
    }
  }

  printf("%td bytes processed.\n", p - input);
  if (p - input != (ptrdiff_t) NELEMS(input))
    rc = result_TEST_FAILED;
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
      printf("difference at %td\n", p - input);
      rc = result_TEST_FAILED;
    }
  }

  printf("%td bytes processed.\n", p - input);
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

/* ----------------------------------------------------------------------- */

/* Compresses 'in' with a 'bufsz' byte buffer and checks the output is 'want'
 * and that it decompresses back to 'in'. */
static result_t packbits_case(const char          *name,
                              const unsigned char *in,
                              size_t               inlen,
                              int                  bufsz,
                              const unsigned char *want,
                              size_t               wantlen)
{
  result_t      rc;
  stream_t     *src;
  stream_t     *comp;
  stream_t     *decomp;
  unsigned char out[512];
  size_t        n;
  int           c;

  src    = NULL;
  comp   = NULL;
  decomp = NULL;

  rc = stream_mem_create(in, inlen, &src);
  if (rc)
    goto Failure;

  rc = stream_packbitscomp_create(src, bufsz, &comp);
  if (rc)
    goto Failure;

  for (n = 0; (c = stream_getc(comp)) != EOF && n < sizeof(out); n++)
    out[n] = (unsigned char) c;

  if (n != wantlen || memcmp(out, want, wantlen) != 0)
  {
    printf("Failure: %s: compressed to %zu bytes, want %zu\n",
           name, n, wantlen);
    rc = result_TEST_FAILED;
    goto Failure;
  }

  stream_destroy(comp);
  comp = NULL;
  stream_destroy(src);
  src = NULL;

  rc = stream_mem_create(want, wantlen, &src);
  if (rc)
    goto Failure;

  rc = stream_packbitsdecomp_create(src, 0, &decomp);
  if (rc)
    goto Failure;

  for (n = 0; (c = stream_getc(decomp)) != EOF && n < sizeof(out); n++)
    out[n] = (unsigned char) c;

  if (n != inlen || memcmp(out, in, inlen) != 0)
  {
    printf("Failure: %s: round trip gave %zu bytes, want %zu\n",
           name, n, inlen);
    rc = result_TEST_FAILED;
    goto Failure;
  }

  stream_destroy(decomp);
  stream_destroy(src);

  return result_OK;


Failure:
  stream_destroy(decomp);
  stream_destroy(comp);
  stream_destroy(src);
  return rc ? rc : result_TEST_FAILED;
}

static result_t test_packbits(void)
{
  static const unsigned char single[] = { 'a', 'b', 'b', 'b', 'b', 'c' };
  static const unsigned char single_out[] =
  {
    0x00, 'a', 0xFD, 'b', 0x00, 'c'
  };
  /* a run of two between literals is cheaper merged into one literal */
  static const unsigned char merge[] = { 'a', 'b', 'b', 'c' };
  static const unsigned char merge_out[] = { 0x03, 'a', 'b', 'b', 'c' };
  /* with a two byte buffer each fill holds one object, so none merge */
  static const unsigned char split_out[] =
  {
    0x00, 'a', 0xFF, 'b', 0x00, 'c'
  };
  static const unsigned char long_out[] = { 0x81, 'x', 0xFF, 'x' };

  result_t      rc;
  unsigned char longrun[130];
  unsigned char literal[129];
  unsigned char literal_out[131];
  int           i;
  stream_t     *src;
  stream_t     *comp;

  printf("Test - PackBits\n");

  rc = packbits_case("literal run literal", single, sizeof(single), 0,
                     single_out, sizeof(single_out));
  if (rc)
    return rc;

  rc = packbits_case("merge", merge, sizeof(merge), 0,
                     merge_out, sizeof(merge_out));
  if (rc)
    return rc;

  rc = packbits_case("tiny buffer", merge, sizeof(merge), 2,
                     split_out, sizeof(split_out));
  if (rc)
    return rc;

  /* runs longer than 128 are split */
  memset(longrun, 'x', sizeof(longrun));
  rc = packbits_case("long run", longrun, sizeof(longrun), 0,
                     long_out, sizeof(long_out));
  if (rc)
    return rc;

  /* literals stop at 128 bytes and the next starts afresh */
  literal_out[0] = 127;
  for (i = 0; i < (int) sizeof(literal); i++)
  {
    literal[i] = (unsigned char) ((i & 1) ? 'b' : 'a');
    if (i < 128)
      literal_out[1 + i] = literal[i];
  }
  literal_out[129] = 0x00;
  literal_out[130] = literal[128];
  rc = packbits_case("long literal", literal, sizeof(literal), 256,
                     literal_out, sizeof(literal_out));
  if (rc)
    return rc;

  /* too small a buffer is refused */
  rc = stream_mem_create(merge, sizeof(merge), &src);
  if (rc)
    return rc;

  rc = stream_packbitscomp_create(src, 1, &comp);
  if (rc != result_BAD_ARG)
  {
    stream_destroy(src);
    return result_TEST_FAILED;
  }

  rc = stream_packbitscomp_create(src, 0, &comp);
  if (rc)
  {
    stream_destroy(src);
    return rc;
  }

  rc = stream_op(comp, stream_IN_MEMORY, NULL);

  stream_destroy(comp);
  stream_destroy(src);

  return rc == result_STREAM_UNKNOWN_OP ? result_OK : result_TEST_FAILED;
}

/* ----------------------------------------------------------------------- */

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

  rc = test_packbits();
  if (rc)
    goto Failure;

  return result_TEST_PASSED;


Failure:
  printf("\n\n*** result=%x\n", rc);
  return result_TEST_FAILED;
}
