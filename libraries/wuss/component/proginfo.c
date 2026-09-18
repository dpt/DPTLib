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

/* One process-wide singleton -- no per-task instances, no create/destroy --
 * rebuilt lazily when the caller's task changes, since the dialogue's window
 * belongs to one "home" task at a time. pending is the desc last passed to
 * wuss_proginfo_set_desc, applied to info by wuss_proginfo_handle_pre_show;
 * it is only read back then, so no copy beyond the struct assignment is
 * needed. */
static struct
{
  wuss_alloc_t         alloc;   /* copied hooks; wuss_t itself not retained */
  wuss_task_t         *task;    /* home task the dialogue was built on */
  wuss_info_t         *info;
  wuss_dialogue_t     *dialogue;
  wuss_proginfo_desc_t pending;
}
g;

/* ----------------------------------------------------------------------- */

/* Fills rows (>= 4 entries) from desc's non-NULL fields, in Name, Purpose,
 * Author, Version order. Shared by proginfo_build (initial layout) and
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
  wuss_info_row_t rows[4];
  int             nrows;

  (void) opaque;

  nrows = proginfo_rows(&g.pending, rows);

  return wuss_info_set_rows(g.info, rows, nrows);
}

/* Free the singleton's owned window/dialogue, leaving g.task cleared so the
 * next call rebuilds from scratch. */
static void proginfo_free(void)
{
  if (g.task == NULL)
    return; /* never built, or already freed -- g.alloc may be unset */

  wuss_dialogue_destroy(g.dialogue);
  wuss_info_destroy(g.info);
  g.dialogue = NULL;
  g.info     = NULL;
  g.task     = NULL;
}

/* (Re)build the singleton's dialogue window on task. */
static result_t proginfo_build(wuss_task_t *task)
{
  result_t        rc;
  wuss_info_row_t rows[4];
  int             nrows;

  proginfo_free();

  g.alloc = task->wuss->alloc;

  nrows = proginfo_rows(&g.pending, rows);
  rc = wuss_info_create(&g.info, task, "About this program", rows, nrows);
  if (rc != result_OK)
    return rc;

  rc = wuss_dialogue_create_on_window(&g.dialogue, task->wuss,
                                      wuss_info_window(g.info),
                                      proginfo_fillout, NULL);
  if (rc != result_OK)
  {
    wuss_info_destroy(g.info);
    g.info = NULL;
    return rc;
  }

  g.task = task;

  return result_OK;
}

wuss_window_t *wuss_proginfo_window(wuss_task_t *task)
{
  if (task == NULL)
    return NULL;

  if (task != g.task)
    if (proginfo_build(task) != result_OK)
      return NULL;

  return wuss_dialogue_window(g.dialogue);
}

void wuss_proginfo_set_desc(const wuss_proginfo_desc_t *desc)
{
  if (desc == NULL)
    return;

  g.pending = *desc;
}

result_t wuss_proginfo_handle_pre_show(void)
{
  return wuss_dialogue_handle_pre_show(g.dialogue);
}
