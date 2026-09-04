/* framebuf/bmfont/enumerate.c -- list the bitmap fonts in a directory */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "base/result.h"
#include "framebuf/bmfont.h"

/* ----------------------------------------------------------------------- */

#define BMFONT_EXT     ".png"
#define BMFONT_EXT_LEN 4

#ifdef TARGET_RISCOS

/* SharedCLibrary (-mlibscl) has no <dirent.h> support, so on RISC OS we
 * enumerate via OSLib's OS_GBPB 9 wrapper instead of opendir/readdir. */

#include "oslib/osgbpb.h"
#include "oslib/os.h"

result_t bmfont_enumerate(const char          *dir,
                          bmfont_enumerate_fn *fn,
                          void                *opaque)
{
  result_t             rc;
  os_error            *err;
  int                  context;
  int                  read;
  char                 buffer[256];
  const char          *leaf;
  size_t               leaflen;
  char                 name[256];
  char                 path[512];

  if (dir == NULL || fn == NULL)
    return result_NULL_ARG;

  rc      = result_OK;
  context = 0;

  for (;;)
  {
    err = xosgbpb_dir_entries(dir,
                              (osgbpb_string_list *) buffer,
                              1,
                              context,
                              sizeof(buffer),
                              "*",
                              &read,
                              &context);
    if (err != NULL)
    {
      rc = result_FILE_NOT_FOUND;
      break;
    }

    if (read > 0)
    {
      leaf    = buffer;
      leaflen = strlen(leaf);

      if (leaflen > BMFONT_EXT_LEN &&
          leaflen - BMFONT_EXT_LEN < sizeof(name) &&
          strcmp(leaf + leaflen - BMFONT_EXT_LEN, BMFONT_EXT) == 0)
      {
        memcpy(name, leaf, leaflen - BMFONT_EXT_LEN);
        name[leaflen - BMFONT_EXT_LEN] = '\0';

        snprintf(path, sizeof(path), "%s.%s", dir, leaf);

        rc = fn(name, path, opaque);
        if (rc == result_STOP_WALK)
        {
          rc = result_OK;
          break;
        }
        if (rc != result_OK)
          break;
      }
    }

    if (context == -1)
      break;
  }

  return rc;
}

#elif defined(_MSC_VER) /* !TARGET_RISCOS */

/* MSVC has no <dirent.h>; enumerate via the Win32 FindFirstFile family
 * instead of opendir/readdir. */

#include <windows.h>

result_t bmfont_enumerate(const char          *dir,
                          bmfont_enumerate_fn *fn,
                          void                *opaque)
{
  result_t             rc;
  WIN32_FIND_DATAA      fd;
  HANDLE                h;
  char                  pattern[512];
  const char           *leaf;
  size_t                leaflen;
  char                  name[256];
  char                  path[512];

  if (dir == NULL || fn == NULL)
    return result_NULL_ARG;

  snprintf(pattern, sizeof(pattern), "%s\\*", dir);

  h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE)
    return result_FILE_NOT_FOUND;

  rc = result_OK;

  do
  {
    leaf    = fd.cFileName;
    leaflen = strlen(leaf);

    if (leaflen <= BMFONT_EXT_LEN || leaflen - BMFONT_EXT_LEN >= sizeof(name))
      continue;
    if (strcmp(leaf + leaflen - BMFONT_EXT_LEN, BMFONT_EXT) != 0)
      continue;

    memcpy(name, leaf, leaflen - BMFONT_EXT_LEN);
    name[leaflen - BMFONT_EXT_LEN] = '\0';

    snprintf(path, sizeof(path), "%s/%s", dir, leaf);

    rc = fn(name, path, opaque);
    if (rc == result_STOP_WALK)
    {
      rc = result_OK;
      break;
    }
    if (rc != result_OK)
      break;
  }
  while (FindNextFileA(h, &fd));

  FindClose(h);

  return rc;
}

#else /* !TARGET_RISCOS, !_MSC_VER */

#include <dirent.h>

result_t bmfont_enumerate(const char          *dir,
                          bmfont_enumerate_fn *fn,
                          void                *opaque)
{
  result_t             rc;
  DIR                 *dp;
  struct dirent       *de;
  const char          *leaf;
  size_t               leaflen;
  char                 name[256];
  char                 path[512];

  if (dir == NULL || fn == NULL)
    return result_NULL_ARG;

  dp = opendir(dir);
  if (dp == NULL)
    return result_FILE_NOT_FOUND;

  rc = result_OK;

  while ((de = readdir(dp)) != NULL)
  {
    leaf    = de->d_name;
    leaflen = strlen(leaf);

    if (leaflen <= BMFONT_EXT_LEN || leaflen - BMFONT_EXT_LEN >= sizeof(name))
      continue;
    if (strcmp(leaf + leaflen - BMFONT_EXT_LEN, BMFONT_EXT) != 0)
      continue;

    memcpy(name, leaf, leaflen - BMFONT_EXT_LEN);
    name[leaflen - BMFONT_EXT_LEN] = '\0';

    snprintf(path, sizeof(path), "%s/%s", dir, leaf);

    rc = fn(name, path, opaque);
    if (rc == result_STOP_WALK)
    {
      rc = result_OK;
      break;
    }
    if (rc != result_OK)
      break;
  }

  closedir(dp);

  return rc;
}

#endif /* TARGET_RISCOS */
