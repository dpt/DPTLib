/* wuss/component/proginfo.c -- a "program information" standard dialogue */

#include <stddef.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"

#include "wuss/task.h"

#include "wuss/component/info.h"
#include "wuss/component/proginfo.h"

/* ----------------------------------------------------------------------- */

/* ponytail: this whole component is now the desc -> rows mapping below;
 * wuss/component/info.c does the window, the measuring and the layout. */

result_t wuss_proginfo_create(wuss_proginfo_t           **out,
                              wuss_task_t                *task,
                              const wuss_proginfo_desc_t *desc)
{
  wuss_info_row_t rows[4];
  int             nrows;

  if (out == NULL || task == NULL || desc == NULL || desc->name == NULL)
    return result_NULL_ARG;

  nrows = 0;
  if (desc->name != NULL)
  {
    rows[nrows].label = "Name";
    rows[nrows].value = desc->name;
    nrows++;
  }
  if (desc->purpose != NULL)
  {
    rows[nrows].label = "Purpose";
    rows[nrows].value = desc->purpose;
    nrows++;
  }
  if (desc->author != NULL)
  {
    rows[nrows].label = "Author";
    rows[nrows].value = desc->author;
    nrows++;
  }
  if (desc->version != NULL)
  {
    rows[nrows].label = "Version";
    rows[nrows].value = desc->version;
    nrows++;
  }

  return wuss_info_create(out, task, "About this program", rows, nrows);
}
