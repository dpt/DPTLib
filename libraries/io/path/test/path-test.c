/* io/path/test/path-test.c */

#include <stdio.h>
#include <string.h>

#include "base/utils.h"
#include "io/path.h"

#include "test/all-tests.h"

static result_t test_plain(void)
{
  const char *r;

  r = pathf("%s/resources/bmfonts/%s.png", "here", "Tiny");

#ifdef __riscos
  if (strcmp(r, "here.resources.bmfonts.Tiny") != 0)
#else
  if (strcmp(r, "here/resources/bmfonts/Tiny.png") != 0)
#endif
  {
    printf("pathf() plain: got \"%s\"\n", r);
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

/* A directory name may itself contain a '.', e.g. a version-numbered or
 * dotfile-style directory. Only the final path component's extension
 * should ever be affected. */
static result_t test_dot_in_directory(void)
{
  const char *r;

  r = pathf("%s/resources.old/v1.2/%s.png", "here", "icon");

#ifdef __riscos
  /* mid-path dots are left alone -- only the leaf's dot is stripped */
  if (strcmp(r, "here.resources.old.v1.2.icon") != 0)
#else
  if (strcmp(r, "here/resources.old/v1.2/icon.png") != 0)
#endif
  {
    printf("pathf() dot-in-directory: got \"%s\"\n", r);
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

/* Opposite case: the final leaf has no dotted extension at all -- nothing
 * should be stripped, even when an earlier component has a dot. */
static result_t test_leaf_without_ext(void)
{
  const char *r;

  r = pathf("%s/v1.2/%s", "here", "icons");

#ifdef __riscos
  if (strcmp(r, "here.v1.2.icons") != 0)
#else
  if (strcmp(r, "here/v1.2/icons") != 0)
#endif
  {
    printf("pathf() leaf-without-ext: got \"%s\"\n", r);
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

/* A bare leafname (no '/' at all) is its own leaf -- the dot-strip must
 * still find it correctly with no preceding separator. */
static result_t test_bare_leaf(void)
{
  const char *r;

  r = pathf("%s.png", "composite-1");

#ifdef __riscos
  if (strcmp(r, "composite-1") != 0)
#else
  if (strcmp(r, "composite-1.png") != 0)
#endif
  {
    printf("pathf() bare-leaf: got \"%s\"\n", r);
    return result_TEST_FAILED;
  }

  return result_TEST_PASSED;
}

result_t path_test(const char *resources)
{
  static result_t (*const tests[])(void) =
  {
    test_plain,
    test_dot_in_directory,
    test_leaf_without_ext,
    test_bare_leaf
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
