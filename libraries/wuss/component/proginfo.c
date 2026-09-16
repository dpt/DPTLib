/* wuss/component/proginfo.c -- a shared "program information" dialogue */

#include <stddef.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"

#include "wuss/task.h"
#include "wuss/window.h"

#include "wuss/component/dialogue.h"
#include "wuss/component/info.h"
#include "wuss/component/proginfo.h"

#include "../core/impl.h"

/* ----------------------------------------------------------------------- */

/* Owns a wuss_info_t (window, layout) wrapped by a wuss_dialogue_t (fillout
 * dispatch only -- wuss_dialogue_create_on_window, so it does not own or
 * close info's window). pending is the desc last passed to
 * wuss_proginfo_set_desc, applied to info by wuss_proginfo_handle_pre_show;
 * it is only read back then, so no copy is needed. */
struct wuss_proginfo
{
  wuss_alloc_t           alloc;   /* copied hooks; wuss_t itself not retained */
  wuss_info_t          *info;
  wuss_dialogue_t       *dialogue;
  wuss_proginfo_desc_t   pending;
};

/* ----------------------------------------------------------------------- */

/* Fills rows (>= 4 entries) from desc's non-NULL fields, in Name, Purpose,
 * Author, Version order. Shared by wuss_proginfo_create (initial layout) and
 * proginfo_fillout (relayout on show). */
static int proginfo_rows(const wuss_proginfo_desc_t *desc,
                         wuss_info_row_t            *rows)
{
  int nrows;

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

  return nrows;
}

static result_t proginfo_fillout(void *opaque)
{
  wuss_proginfo_t *pi;
  wuss_info_row_t  rows[4];
  int              nrows;

  pi    = opaque;
  nrows = proginfo_rows(&pi->pending, rows);

  return wuss_info_set_rows(pi->info, rows, nrows);
}

result_t wuss_proginfo_create(wuss_proginfo_t           **out,
                              wuss_task_t                *task,
                              const wuss_proginfo_desc_t *desc)
{
  result_t         rc;
  wuss_proginfo_t *pi;
  wuss_info_row_t  rows[4];
  int              nrows;

  if (out == NULL || task == NULL || desc == NULL || desc->name == NULL)
    return result_NULL_ARG;

  pi = task->wuss->alloc.malloc(sizeof(*pi));
  if (pi == NULL)
    return result_OOM;
  pi->alloc   = task->wuss->alloc;
  pi->pending = *desc;

  nrows = proginfo_rows(desc, rows);
  rc = wuss_info_create(&pi->info, task, "About this program", rows, nrows);
  if (rc != result_OK)
  {
    task->wuss->alloc.free(pi);
    return rc;
  }

  rc = wuss_dialogue_create_on_window(&pi->dialogue, task->wuss,
                                      wuss_info_window(pi->info),
                                      proginfo_fillout, pi);
  if (rc != result_OK)
  {
    wuss_info_destroy(pi->info);
    task->wuss->alloc.free(pi);
    return rc;
  }

  *out = pi;
  return result_OK;
}

void wuss_proginfo_destroy(wuss_proginfo_t *doomed)
{
  wuss_alloc_t alloc;

  if (doomed == NULL)
    return;

  alloc = doomed->alloc;
  wuss_dialogue_destroy(doomed->dialogue);
  wuss_info_destroy(doomed->info);
  alloc.free(doomed);
}

void wuss_proginfo_set_desc(wuss_proginfo_t            *pi,
                            const wuss_proginfo_desc_t *desc)
{
  if (pi == NULL || desc == NULL)
    return;

  pi->pending = *desc;
}

result_t wuss_proginfo_handle_pre_show(wuss_proginfo_t *pi)
{
  return wuss_dialogue_handle_pre_show(pi->dialogue);
}

wuss_window_t *wuss_proginfo_window(const wuss_proginfo_t *pi)
{
  return pi ? wuss_dialogue_window(pi->dialogue) : NULL;
}
