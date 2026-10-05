/* taskmanager.c -- permanent icon bar "Tasks" icon owning the Wuss menu */

#ifdef WUSS_ICONBAR

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "io/path.h"
#include "wuss/iconbar.h"
#include "wuss/task.h"

#include "taskmanager.h"
#include "tasks.h"

typedef struct
{
  bitmap_t glyph;
  int      have_glyph;
}
taskmanager_t;

static result_t taskmanager_handle(wuss_window_t      *window,
                                   const wuss_event_t *event,
                                   void               *task_data)
{
  taskmanager_t *task;

  task = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_ICON:
    if (window != NULL || event->data.iconbar_icon.action != wuss_MOUSE_UP)
      return result_OK;
    if (event->data.iconbar_icon.button & wuss_BUTTON_MENU)
      return tasks_open_menu();
    return result_OK; /* SELECT and ADJUST: nothing yet */

  case wuss_EVENT_QUIT:
    if (task->have_glyph)
      free(task->glyph.base);
    free(task);
    return result_OK;

  default:
    return result_OK;
  }
}

result_t taskmanager_create(wuss_t *wuss)
{
  result_t                 rc;
  taskmanager_t           *task;
  wuss_task_t             *wtask;
  wuss_task_desc_t         desc;
  wuss_iconbar_icon_spec_t spec;
  wuss_iconbar_icon_t     *icon;
  const char              *path;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  path = pathf("%s/resources/wuss/files/file_app.png",
               wuss_get_resources(wuss));
  task->have_glyph = bitmap_load_png(&task->glyph, path) == result_OK;
  if (!task->have_glyph)
    logf_warning("taskmanager: glyph '%s' unavailable; icon shows label only",
                 path);

  desc.handle    = taskmanager_handle;
  desc.task_data = task;
  desc.name      = "taskmanager";
  rc = wuss_task_create(wuss, &desc, &wtask);
  if (rc != result_OK)
  {
    if (task->have_glyph)
      free(task->glyph.base);
    free(task);
    return rc;
  }

  spec.text  = "Tasks";
  spec.side  = wuss_ICONBAR_RIGHT;
  spec.image = task->have_glyph ? &task->glyph : NULL;
  rc = wuss_iconbar_icon_create(wuss, wtask, &spec, &icon);
  if (rc != result_OK)
    wuss_task_destroy(wtask); /* its QUIT frees task */

  return rc;
}

#endif /* WUSS_ICONBAR */
