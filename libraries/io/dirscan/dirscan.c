/* io/dirscan.c -- platform-independent flat directory scan */

#include <stddef.h>
#include <string.h>

#include "base/debug.h"
#include "base/result.h"
#include "io/dirscan.h"

/* ----------------------------------------------------------------------- */

#ifdef TARGET_RISCOS

/* SharedCLibrary (-mlibscl) has no <dirent.h> support, so on RISC OS we
 * enumerate via OSLib's OS_GBPB 10 wrapper instead of opendir/readdir --
 * catalogue info is needed (not just the leafname, OS_GBPB 9) to filter out
 * directories. */

#include "oslib/osgbpb.h"
#include "oslib/os.h"

result_t dirscan_walk(const char *dir, dirscan_fn *fn, void *opaque)
{
  result_t          rc;
  os_error         *err;
  osgbpb_info_list *list;
  int               context;
  int               read;
  char              buffer[256];

  if (dir == NULL || fn == NULL)
    return result_NULL_ARG;

  rc      = result_OK;
  context = 0;
  list    = (osgbpb_info_list *) buffer;

  for (;;)
  {
    err = xosgbpb_dir_entries_info(dir, list, 1, context, sizeof(buffer),
                                   "*", &read, &context);
    if (err != NULL)
    {
      logf_error("dirscan_walk: xosgbpb_dir_entries_info(\"%s\") -> "
                "0x%x \"%s\"", dir, err->errnum, err->errmess);

      /* Nothing read yet: the directory itself couldn't be opened -- a
       * real failure. Partway through: something about one entry upset
       * FileSwitch (e.g. a foreign filesystem exposing a leafname RISC OS
       * pathnames can't represent, such as a HostFS-shared ".DS_Store").
       * The SWI gives no documented way to skip just that entry and
       * resume, so stop the walk here but report what was found rather
       * than failing the whole scan. */
      rc = (context == 0) ? result_FILE_NOT_FOUND : result_OK;
      break;
    }

    /* Directories (and anything else that isn't a plain file, e.g. image
     * filing system objects) don't belong in a flat leafname walk -- a
     * caller like bmfont_enumerate would otherwise try to open one as a
     * font and fail with "file not found". Likewise a leafname starting
     * '/' -- RISC OS's escaped form of a foreign filesystem's leading '.'
     * (e.g. a HostFS-shared ".DS_Store") -- is never something a real
     * RISC OS filesystem would hand us, and re-reading it via a pathname
     * later is exactly what upsets FileSwitch enough to abort the scan
     * (see the error case above). */
    if (read > 0 && list->info[0].obj_type == fileswitch_IS_FILE &&
        list->info[0].name[0] != '/')
    {
      rc = fn(list->info[0].name, opaque);
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
