/* test/tmpdir/tmpdir.c -- scratch directory for tests that write files */

#include <stdio.h>
#include <stdlib.h>

#include "test/tmpdir.h"

const char *test_tmpdir(void)
{
#if defined(_WIN32)
  const char *d;

  d = getenv("TEMP");
  if (d == NULL || d[0] == '\0')
    d = getenv("TMP");
  if (d == NULL || d[0] == '\0')
    d = ".";
  return d;
#elif defined(__riscos)
  return ".";
#else
  return "/tmp";
#endif
}

int test_tmppath(char *buf, size_t cap, const char *leaf)
{
  return snprintf(buf, cap, "%s/%s", test_tmpdir(), leaf) < (int) cap;
}

/* vim: set ts=8 sts=2 sw=2 et: */
