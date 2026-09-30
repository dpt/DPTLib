/* io/filetype/test/filetype-test.c */

#include <string.h>

#include "base/utils.h"
#include "io/filetype.h"

#include "test/all-tests.h"

static result_t test_from_ext_known(void)
{
  filetype_t t;

  t = filetype_from_ext(".png");
  if (t.riscos != 0xB60 || strcmp(t.ext, ".png") != 0)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

static result_t test_from_ext_case_insensitive(void)
{
  filetype_t t;

  t = filetype_from_ext(".PNG");
  if (t.riscos != 0xB60)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

static result_t test_from_ext_unknown(void)
{
  filetype_t t;

  t = filetype_from_ext(".xyz");
  if (t.riscos != filetype_DATA.riscos ||
      strcmp(t.ext, filetype_DATA.ext) != 0)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

static result_t test_from_ext_null(void)
{
  filetype_t t;

  t = filetype_from_ext(NULL);
  if (t.riscos != filetype_DATA.riscos)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

static result_t test_from_riscos_known(void)
{
  filetype_t t;

  t = filetype_from_riscos(0xFFF);
  if (strcmp(t.ext, ".txt") != 0)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

/* An unrecognised RISC OS number is carried through unchanged rather than
 * being silently rewritten to filetype_DATA's own number -- round-tripping
 * a number the table doesn't know must not lose it. */
static result_t test_from_riscos_unknown(void)
{
  filetype_t t;

  t = filetype_from_riscos(0x1234);
  if (t.riscos != 0x1234 || strcmp(t.ext, "") != 0)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

static result_t test_set_off_riscos(void)
{
  result_t rc;

  /* Off RISC OS, filetype_set is a no-op: it must not fail just because
   * the path doesn't exist. */
  rc = filetype_set("/no/such/path", filetype_from_ext(".png"));
#ifndef __riscos
  if (rc != result_OK)
    return result_TEST_FAILED;
#else
  NOT_USED(rc);
#endif

  return result_TEST_PASSED;
}

static result_t test_set_null_path(void)
{
  result_t rc;

  rc = filetype_set(NULL, filetype_DATA);
  if (rc != result_NULL_ARG)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

result_t filetype_test(const char *resources)
{
  static result_t (*const tests[])(void) =
  {
    test_from_ext_known,
    test_from_ext_case_insensitive,
    test_from_ext_unknown,
    test_from_ext_null,
    test_from_riscos_known,
    test_from_riscos_unknown,
    test_set_off_riscos,
    test_set_null_path
  };

  result_t rc;
  size_t   i;
  int      nfailures;

  NOT_USED(resources);

  nfailures = 0;
  for (i = 0; i < NELEMS(tests); i++)
  {
    rc = tests[i]();
    if (rc != result_TEST_PASSED)
      nfailures++;
  }

  return (nfailures == 0) ? result_TEST_PASSED : result_TEST_FAILED;
}
