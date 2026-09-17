/* io/path/path.c */

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "base/utils.h"
#include "io/path.h"

const char *pathf(const char *fmt, ...)
{
  static char buf[DPTLIB_MAXPATH];

  va_list     args;

  assert(fmt);

  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

#ifdef __riscos

  /* Strip a dotted extension from the final path component only -- RISC OS
   * has no extension in the string at all, so translating mid-path '/'
   * separators to '.' below would otherwise leave it looking like one more
   * directory level. */
  {
    char *leaf;
    char *dot;
    char *p;

    leaf = strrchr(buf, '/');
    leaf = (leaf != NULL) ? leaf + 1 : buf;
    dot  = strrchr(leaf, '.');
    if (dot != NULL)
      *dot = '\0';

    for (p = buf; *p != '\0'; p++)
      if (*p == '/')
        *p = '.';
  }

#endif

  return buf;
}

int path_leaf_strip_ext(const char *leaf,
                        const char *ext,
                        char       *name,
                        size_t      cap)
{
  assert(leaf);
  assert(ext);
  assert(name);

#ifdef __riscos

  {
    size_t leaflen;

    /* No extension in the string to match against -- filetype is separate
     * metadata -- so every leaf matches, unchanged. */
    NOT_USED(ext);

    leaflen = strlen(leaf);
    if (leaflen + 1 > cap)
      return 0;

    memcpy(name, leaf, leaflen + 1);

    return 1;
  }

#else

  {
    size_t leaflen;
    size_t extlen;

    leaflen = strlen(leaf);
    extlen  = strlen(ext);

    if (leaflen <= extlen || leaflen - extlen >= cap)
      return 0;
    if (strcmp(leaf + leaflen - extlen, ext) != 0)
      return 0;

    memcpy(name, leaf, leaflen - extlen);
    name[leaflen - extlen] = '\0';

    return 1;
  }

#endif
}
