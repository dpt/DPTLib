/* io/filetype/filetype.c -- file type identification and RISC OS filetype mapping */

#include <string.h>
#include <strings.h>

#ifdef __riscos
#include "kernel.h"
#include "swis.h"
#endif

#include "base/result.h"
#include "base/utils.h"

#include "io/filetype.h"

/* ----------------------------------------------------------------------- */

const filetype_t filetype_DATA = { 0xFFD, "" };

/* Directory (0xFFD/"") deliberately duplicates filetype_DATA -- kept in the
 * table so filetype_from_riscos(0xFFD) returns it via the normal scan. */
static const filetype_t g_filetypes[] =
{
  { 0xB60, ".png"  },
  { 0xFFF, ".txt"  },
  { 0xFFD, ""      }  /* directory has no host extension of its own */
};

#define NFILETYPES (sizeof(g_filetypes) / sizeof(g_filetypes[0]))

/* ----------------------------------------------------------------------- */

filetype_t filetype_from_ext(const char *ext)
{
  size_t i;

  if (ext == NULL)
    return filetype_DATA;

  for (i = 0; i < NFILETYPES; i++)
    if (g_filetypes[i].ext[0] != '\0' &&
        strcasecmp(g_filetypes[i].ext, ext) == 0)
      return g_filetypes[i];

  return filetype_DATA;
}

filetype_t filetype_from_riscos(unsigned short riscos)
{
  size_t i;

  for (i = 0; i < NFILETYPES; i++)
    if (g_filetypes[i].riscos == riscos)
      return g_filetypes[i];

  return (filetype_t) { riscos, filetype_DATA.ext };
}

result_t filetype_set(const char *path, filetype_t type)
{
  if (path == NULL)
    return result_NULL_ARG;

#ifdef __riscos

  {
    _kernel_oserror *err;

    /* OS_File 18: set a file's type. */
    err = _swix(OS_File, _INR(0,2), 18, path, type.riscos);
    if (err != NULL)
      return result_FILE_NOT_FOUND;
  }

#else

  NOT_USED(type);

#endif

  return result_OK;
}
