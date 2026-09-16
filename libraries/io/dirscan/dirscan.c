/* io/dirscan.c -- platform-independent flat directory scan */

#include <stddef.h>
#include <string.h>

#include "base/result.h"
#include "io/dirscan.h"

/* ----------------------------------------------------------------------- */

#ifdef TARGET_RISCOS

/* SharedCLibrary (-mlibscl) has no <dirent.h> support, so on RISC OS we
 * enumerate via OSLib's OS_GBPB 9 wrapper instead of opendir/readdir. */

#include "oslib/osgbpb.h"
#include "oslib/os.h"

result_t dirscan_walk(const char *dir, dirscan_fn *fn, void *opaque)
{
  result_t  rc;
  os_error *err;
  int       context;
  int       read;
  char      buffer[256];

  if (dir == NULL || fn == NULL)
    return result_NULL_ARG;

  rc      = result_OK;
  context = 0;

  for (;;)
  {
    err = xosgbpb_dir_entries(dir, (osgbpb_string_list *) buffer, 1, context,
                              sizeof(buffer), "*", &read, &context);
    if (err != NULL)
    {
      rc = result_FILE_NOT_FOUND;
      break;
    }

    if (read > 0)
    {
      rc = fn(buffer, opaque);
      if (rc == result_STOP_WALK)
      {
        rc = result_OK;
        break;
      }
      if (rc != result_OK)
        break;
    }

    if (context == -1)
      break;
  }

  return rc;
}

#elif defined(_MSC_VER) /* !TARGET_RISCOS */

/* MSVC has no <dirent.h>; enumerate via the Win32 FindFirstFile family
 * instead of opendir/readdir. */

#include <stdio.h>
#include <windows.h>

#include "io/path.h"

result_t dirscan_walk(const char *dir, dirscan_fn *fn, void *opaque)
{
  result_t         rc;
  WIN32_FIND_DATAA fd;
  HANDLE           h;
  char             pattern[DPTLIB_MAXPATH];

  if (dir == NULL || fn == NULL)
    return result_NULL_ARG;

  snprintf(pattern, sizeof(pattern), "%s\\*", dir);

  h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE)
    return result_FILE_NOT_FOUND;

  rc = result_OK;

  do
  {
    rc = fn(fd.cFileName, opaque);
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

result_t dirscan_walk(const char *dir, dirscan_fn *fn, void *opaque)
{
  result_t rc;
  DIR     *dp;
  struct dirent *de;

  if (dir == NULL || fn == NULL)
    return result_NULL_ARG;

  dp = opendir(dir);
  if (dp == NULL)
    return result_FILE_NOT_FOUND;

  rc = result_OK;

  while ((de = readdir(dp)) != NULL)
  {
    rc = fn(de->d_name, opaque);
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
