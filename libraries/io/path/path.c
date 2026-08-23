/* path.c */

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "base/utils.h"
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
  size_t      used;
  int         rc;

  assert(root);
  assert(nbranches < 1000);

  va_start(args, nbranches);

  rc = snprintf(buf, sizeof(buf), "%s", root);
  used = (rc < 0) ? sizeof(buf) : MIN((size_t) rc, sizeof(buf) - 1);

  while (nbranches-- && used < sizeof(buf) - 1)
  {
    rc = snprintf(buf + used, sizeof(buf) - used, "%s%s",
                  sep, va_arg(args, const char *));
    used += (rc < 0) ? 0 : MIN((size_t) rc, sizeof(buf) - used - 1);
  }

  va_end(args);

  return buf;
}
