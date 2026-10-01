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
   * directory level.
   *
   * No '/' at all means there is no Unix-style leaf to strip: the string may
   * already be a native path, where '.' is the directory separator, so it is
   * left alone. */
  {
    char *leaf;
    char *dot;
    char *p;

    leaf = strrchr(buf, '/');
    if (leaf != NULL)
    {
      dot = strrchr(leaf + 1, '.');
      if (dot != NULL)
        *dot = '\0';
    }

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

int path_leaf_valid(const char *leaf)
{
  if (leaf == NULL || leaf[0] == '\0')
    return 0;

  if (strcmp(leaf, ".") == 0 || strcmp(leaf, "..") == 0)
    return 0;

#ifdef __riscos
  if (strchr(leaf, '.') != NULL)
    return 0;
#else
  if (strchr(leaf, '/') != NULL)
    return 0;
#ifdef _WIN32
  if (strchr(leaf, '\\') != NULL)
    return 0;
#endif
#endif

  return 1;
}

const char *path_leaf(const char *path)
{
  const char *leaf;
  const char *p;

  assert(path);

  leaf = path;
  for (p = path; *p != '\0'; p++)
  {
#ifdef __riscos
    if (*p == '.' || *p == ':')
#elif defined(_WIN32)
    if (*p == '/' || *p == '\\')
#else
    if (*p == '/')
#endif
      leaf = p + 1;
  }

  return leaf;
}

int path_is_full(const char *path)
{
  if (path == NULL || path[0] == '\0')
    return 0;

#ifdef __riscos

  if (path[0] == '$')
    return 1;
  if (strstr(path, "::") != NULL)
    return 1;

  return 0;

#else

#ifdef _WIN32
  if (((path[0] >= 'A' && path[0] <= 'Z') ||
       (path[0] >= 'a' && path[0] <= 'z')) &&
      path[1] == ':' &&
      (path[2] == '\\' || path[2] == '/'))
    return 1;
#endif

  return path[0] == '/';

#endif
}
