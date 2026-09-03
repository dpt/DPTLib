/* framebuf/bmfont/enumerate.c -- list the bitmap fonts in a directory */

#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "base/result.h"
#include "framebuf/bmfont.h"

/* ----------------------------------------------------------------------- */

#define BMFONT_EXT     ".png"
#define BMFONT_EXT_LEN 4

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
