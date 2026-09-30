/* io/dirlist/test/dirlist-test.c */

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "base/utils.h"
#include "io/dirlist.h"

#include "test/all-tests.h"

/* Filesystem-backed tests need a scratch directory to scan; only wired up
 * for the desktop POSIX build (mkdtemp), which is what build-asan runs.
 * ponytail: no Windows/RISC OS scratch-dir helper yet, add one if these
 * tests need to run there too. */
#if !defined(_WIN32) && !defined(__riscos)
#define DIRLIST_TEST_HAS_FS 1
#endif

#ifdef DIRLIST_TEST_HAS_FS

#include <stdlib.h>
#include <sys/stat.h>

static int make_scratch_dir(char *path, size_t cap)
{
  if (snprintf(path, cap, "/tmp/dptlib-dirlist-XXXXXX") >= (int) cap)
    return 0;

  return mkdtemp(path) != NULL;
}

static int touch(const char *dir, const char *leaf)
{
  char  path[512];
  FILE *fp;

  snprintf(path, sizeof(path), "%s/%s", dir, leaf);
  fp = fopen(path, "wb");
  if (fp == NULL)
    return 0;
  fclose(fp);

  return 1;
}

static void remove_scratch_dir(const char         *dir,
                               const char * const *leaves,
                               int                 nleaves)
{
  char path[512];
  int  i;

  for (i = 0; i < nleaves; i++)
  {
    snprintf(path, sizeof(path), "%s/%s", dir, leaves[i]);
    remove(path);
  }

  rmdir(dir);
}

static result_t test_scan_sorted_and_filtered(void)
{
  static const char *const files[] = { "banana.txt", "Apple.png",
                                       ".hidden" };

  result_t               rc;
  dirlist_t             *list;
  char                   dir[64];
  char                   subdir[96];
  const dirlist_entry_t *e;

  rc   = result_TEST_FAILED;
  list = NULL;

  if (!make_scratch_dir(dir, sizeof(dir)))
    return result_TEST_FAILED;

  if (!touch(dir, files[0]) || !touch(dir, files[1]) || !touch(dir, files[2]))
    goto cleanup;

  snprintf(subdir, sizeof(subdir), "%s/zzz", dir);
  if (mkdir(subdir, 0700) != 0)
    goto cleanup;

  if (dirlist_scan(dir, &list) != result_OK)
    goto cleanup;

  /* Dotfile filtered out: 3 entries (Apple.png, banana.txt, zzz), not 4. */
  if (dirlist_count(list) != 3)
    goto cleanup;

  /* Sorted case-insensitively: Apple before banana before zzz. */
  e = dirlist_get(list, 0);
  if (e == NULL || strcmp(e->leaf, "Apple.png") != 0 || e->is_dir)
    goto cleanup;
  if (e->type.riscos != 0xB60)
    goto cleanup;

  e = dirlist_get(list, 1);
  if (e == NULL || strcmp(e->leaf, "banana.txt") != 0 || e->is_dir)
    goto cleanup;

  e = dirlist_get(list, 2);
  if (e == NULL || strcmp(e->leaf, "zzz") != 0 || !e->is_dir)
    goto cleanup;

  rc = result_TEST_PASSED;

cleanup:
  dirlist_destroy(list);
  rmdir(subdir);
  remove_scratch_dir(dir, files, NELEMS(files));

  return rc;
}

static result_t test_scan_missing_dir(void)
{
  result_t   rc;
  dirlist_t *list;

  list = NULL;

  rc = dirlist_scan("/no/such/dptlib/dir", &list);
  if (rc != result_FILE_NOT_FOUND)
    return result_TEST_FAILED;

  return result_TEST_PASSED;
}

#endif /* DIRLIST_TEST_HAS_FS */

static result_t test_null_args(void)
{
  dirlist_t *list;

  if (dirlist_scan(NULL, &list) != result_NULL_ARG)
    return result_TEST_FAILED;
  if (dirlist_scan(".", NULL) != result_NULL_ARG)
    return result_TEST_FAILED;

  if (dirlist_count(NULL) != 0)
    return result_TEST_FAILED;
  if (dirlist_get(NULL, 0) != NULL)
    return result_TEST_FAILED;

  dirlist_destroy(NULL); /* must not crash */

  return result_TEST_PASSED;
}

result_t dirlist_test(const char *resources)
{
  static result_t (*const tests[])(void) =
  {
    test_null_args,
#ifdef DIRLIST_TEST_HAS_FS
    test_scan_sorted_and_filtered,
    test_scan_missing_dir
#endif
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
