/* path.c */

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "io/path.h"

const char *path_join_leafname(const char *leaf, const char *ext)
{
  static char buf[DPTLIB_MAXPATH];

  const char *fmt =
#ifdef __riscos
    "%s/%s";
#else
    "%s.%s";
#endif

  assert(leaf);
  assert(ext);

  snprintf(buf, sizeof(buf), fmt, leaf, ext);

  return buf;
}

const char *path_join_filename(const char *root, int nbranches, ...)
{
  static char buf[DPTLIB_MAXPATH];

  const char *sep =
#ifdef __riscos
    ".";
#else
    "/";
#endif
  va_list     args;

  assert(root);
  assert(nbranches < 1000);

  va_start(args, nbranches);
  strncpy(buf, root, sizeof(buf));
  while (nbranches--)
  {
    strcat(buf, sep);
    strcat(buf, va_arg(args, const char *));
  }
  va_end(args);

  return buf;
}
