/* io/path/test/path-test.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

static result_t test_leaf_valid_accepts(void)
{
  if (!path_leaf_valid("gradient.png") || !path_leaf_valid("a"))
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

static result_t test_leaf_valid_rejects(void)
{
  if (path_leaf_valid(NULL)  ||
      path_leaf_valid("")    ||
      path_leaf_valid(".")   ||
      path_leaf_valid("..")
#ifdef __riscos
   || path_leaf_valid("a.b")   /* '.' is the RISC OS separator */
#else
   || path_leaf_valid("a/b")
   || path_leaf_valid("../x")
#endif
     )
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

static result_t test_leaf(void)
{
#ifdef __riscos
  if (strcmp(path_leaf("ADFS::disc.$.dir.file"), "file") != 0 ||
      strcmp(path_leaf("file"), "file") != 0)
    return result_TEST_FAILED;
#else
  if (strcmp(path_leaf("./dir/gradient.png"), "gradient.png") != 0 ||
      strcmp(path_leaf("gradient.png"), "gradient.png") != 0 ||
      strcmp(path_leaf("dir/"), "") != 0)
    return result_TEST_FAILED;
#endif

  return result_TEST_PASSED;
}

static result_t test_is_full(void)
{
#ifdef __riscos
  if (!path_is_full("$.dir.file") || !path_is_full("ADFS::disc.$.dir"))
    return result_TEST_FAILED;
  if (path_is_full("dir.file") || path_is_full(NULL) || path_is_full(""))
    return result_TEST_FAILED;
#else
  if (!path_is_full("/dir/file"))
    return result_TEST_FAILED;
  if (path_is_full("dir/file") || path_is_full(NULL) || path_is_full(""))
    return result_TEST_FAILED;
#endif

  return result_TEST_PASSED;
}

static result_t test_parent(void)
{
  char out[16];

#ifdef __riscos
  if (!path_parent("ADFS::disc.$.dir", out, sizeof(out)) ||
      strcmp(out, "ADFS::disc.$") != 0)
    return result_TEST_FAILED;
  if (path_parent("file", out, sizeof(out)) ||
      path_parent("ADFS::disc.$.dir", out, 4))
    return result_TEST_FAILED;
#else
  if (!path_parent("/dir/file", out, sizeof(out)) ||
      strcmp(out, "/dir") != 0)
    return result_TEST_FAILED;
  if (!path_parent("/dir", out, sizeof(out)) || strcmp(out, "/") != 0)
    return result_TEST_FAILED;
  if (path_parent("/", out, sizeof(out)) ||
      path_parent("file", out, sizeof(out)) ||
      path_parent("/dir/file", out, 4))
    return result_TEST_FAILED;
#endif

  return result_TEST_PASSED;
}

/* point HOME at a scratch directory so the real one is never touched */
static result_t test_user_data(void)
{
#if defined(__riscos) || defined(_WIN32)
  return result_TEST_PASSED;
#else
  static const char home[] = DPTLIB_TMPPREFIX "dptlib-path-test";
#ifdef __APPLE__
  static const char want[] = DPTLIB_TMPPREFIX
                             "dptlib-path-test/Library/Application Support/Test";
#else
  static const char want[] = DPTLIB_TMPPREFIX
                             "dptlib-path-test/.local/share/test";
#endif

  char        saved_home[DPTLIB_MAXPATH];
  char        saved_xdg[DPTLIB_MAXPATH];
  const char *env;
  char        out[DPTLIB_MAXPATH];
  int         ok;
  struct stat st;

  env = getenv("HOME");
  snprintf(saved_home, sizeof(saved_home), "%s", env ? env : "");
  env = getenv("XDG_DATA_HOME");
  snprintf(saved_xdg, sizeof(saved_xdg), "%s", env ? env : "");

  setenv("HOME", home, 1);
  unsetenv("XDG_DATA_HOME");

  ok = path_user_data("Test", out, sizeof(out)) &&
       strcmp(out, want) == 0 &&
       stat(out, &st) == 0 && S_ISDIR(st.st_mode) &&
       !path_user_data("Test", out, 8);

  setenv("HOME", saved_home, 1);
  if (saved_xdg[0] != '\0')
    setenv("XDG_DATA_HOME", saved_xdg, 1);

  return ok ? result_TEST_PASSED : result_TEST_FAILED;
#endif
}

result_t path_test(const char *resources)
{
  static result_t (*const tests[])(void) =
  {
    test_plain,
    test_dot_in_directory,
    test_leaf_without_ext,
    test_bare_leaf,
    test_leaf_valid_accepts,
    test_leaf_valid_rejects,
    test_leaf,
    test_is_full,
    test_parent,
    test_user_data
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
