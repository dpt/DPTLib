/* framebuf/bmfont/test/bmfontcache-test.c */

#include <stdio.h>

#include "base/utils.h"
#include "framebuf/bmfontcache.h"
#include "io/path.h"

#include "test/all-tests.h"

static result_t test_acquire_dedups(const char *resources)
{
  result_t       rc;
  bmfontcache_t *cache;
  const char    *path;
  bmfont_t      *a, *b;

  rc = bmfontcache_create(&cache);
  if (rc != result_OK)
  {
    printf("bmfontcache_create: rc=%d\n", rc);
    return result_TEST_FAILED;
  }

  path = pathf("%s/resources/bmfonts/Tiny.png", resources);

  rc = bmfontcache_acquire(cache, path, &a);
  if (rc != result_OK)
  {
    printf("bmfontcache_acquire (1st): rc=%d\n", rc);
    bmfontcache_destroy(cache);
    return result_TEST_FAILED;
  }

  rc = bmfontcache_acquire(cache, path, &b);
  if (rc != result_OK)
  {
    printf("bmfontcache_acquire (2nd): rc=%d\n", rc);
    bmfontcache_release(cache, a);
    bmfontcache_destroy(cache);
    return result_TEST_FAILED;
  }

  if (a != b)
  {
    printf("bmfontcache_acquire: same path returned different instances\n");
    bmfontcache_release(cache, a);
    bmfontcache_release(cache, b);
    bmfontcache_destroy(cache);
    return result_TEST_FAILED;
  }

  bmfontcache_release(cache, a);
  bmfontcache_release(cache, b);
  bmfontcache_destroy(cache);

  return result_TEST_PASSED;
}

static result_t test_release_reloads(const char *resources)
{
  result_t       rc;
  bmfontcache_t *cache;
  const char    *path;
  bmfont_t      *a, *b;

  rc = bmfontcache_create(&cache);
  if (rc != result_OK)
  {
    printf("bmfontcache_create: rc=%d\n", rc);
    return result_TEST_FAILED;
  }

  path = pathf("%s/resources/bmfonts/Tiny.png", resources);

  rc = bmfontcache_acquire(cache, path, &a);
  if (rc != result_OK)
  {
    printf("bmfontcache_acquire (1st): rc=%d\n", rc);
    bmfontcache_destroy(cache);
    return result_TEST_FAILED;
  }

  bmfontcache_release(cache, a); /* last holder: font is dropped */

  rc = bmfontcache_acquire(cache, path, &b);
  if (rc != result_OK)
  {
    printf("bmfontcache_acquire (after release): rc=%d\n", rc);
    bmfontcache_destroy(cache);
    return result_TEST_FAILED;
  }

  bmfontcache_release(cache, b);
  bmfontcache_destroy(cache);

  return result_TEST_PASSED;
}

static result_t test_independent_paths(const char *resources)
{
  result_t       rc;
  bmfontcache_t *cache;
  char           path_a[DPTLIB_MAXPATH];
  const char    *path_b;
  bmfont_t      *a, *b;

  rc = bmfontcache_create(&cache);
  if (rc != result_OK)
  {
    printf("bmfontcache_create: rc=%d\n", rc);
    return result_TEST_FAILED;
  }

  /* pathf() returns a pointer to a static buffer, so the first path must be
   * copied out before the second call to pathf() overwrites it. */
  snprintf(path_a, sizeof(path_a), "%s",
          pathf("%s/resources/bmfonts/Tiny.png", resources));
  path_b = pathf("%s/resources/bmfonts/Symbols.png", resources);

  rc = bmfontcache_acquire(cache, path_a, &a);
  if (rc != result_OK)
  {
    printf("bmfontcache_acquire (a): rc=%d\n", rc);
    bmfontcache_destroy(cache);
    return result_TEST_FAILED;
  }

  rc = bmfontcache_acquire(cache, path_b, &b);
  if (rc != result_OK)
  {
    printf("bmfontcache_acquire (b): rc=%d\n", rc);
    bmfontcache_release(cache, a);
    bmfontcache_destroy(cache);
    return result_TEST_FAILED;
  }

  if (a == b)
  {
    printf("bmfontcache_acquire: different paths returned same instance\n");
    bmfontcache_release(cache, a);
    bmfontcache_release(cache, b);
    bmfontcache_destroy(cache);
    return result_TEST_FAILED;
  }

  bmfontcache_release(cache, a);
  bmfontcache_release(cache, b);
  bmfontcache_destroy(cache);

  return result_TEST_PASSED;
}

/* bmfontcache_destroy must cope with fonts still held (see its docs). */
static result_t test_destroy_with_outstanding_acquire(const char *resources)
{
  result_t       rc;
  bmfontcache_t *cache;
  const char    *path;
  bmfont_t      *a;

  rc = bmfontcache_create(&cache);
  if (rc != result_OK)
  {
    printf("bmfontcache_create: rc=%d\n", rc);
    return result_TEST_FAILED;
  }

  path = pathf("%s/resources/bmfonts/Tiny.png", resources);

  rc = bmfontcache_acquire(cache, path, &a);
  if (rc != result_OK)
  {
    printf("bmfontcache_acquire: rc=%d\n", rc);
    bmfontcache_destroy(cache);
    return result_TEST_FAILED;
  }

  bmfontcache_destroy(cache); /* no release() -- must not crash/leak */

  return result_TEST_PASSED;
}

result_t bmfontcache_test(const char *resources)
{
  static result_t (*const tests[])(const char *) =
  {
    test_acquire_dedups,
    test_release_reloads,
    test_independent_paths,
    test_destroy_with_outstanding_acquire
  };

  result_t rc;
  size_t   i;
  int      nfailures;

  nfailures = 0;
  for (i = 0; i < NELEMS(tests); i++)
  {
    rc = tests[i](resources);
    if (rc != result_TEST_PASSED)
      nfailures++;
  }

  return (nfailures == 0) ? result_TEST_PASSED : result_TEST_FAILED;
}
