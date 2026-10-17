/* taskmanager.c -- permanent icon bar "Tasks" icon owning the Wuss menu */

#ifdef WUSS_ICONBAR

#include "base/debug.h"
#include "base/utils.h"
#include "framebuf/bitmap.h"
#include "wuss/iconbar.h"
#include "wuss/task.h"

#include "taskmanager.h"
#include "tasks.h"

static result_t taskmanager_handle(wuss_window_t      *window,
                                   const wuss_event_t *event,
                                   void               *task_data)
{
  (void) task_data;

  if (event->kind != wuss_EVENT_ICON)
    return result_OK;

  if (window != NULL || event->data.iconbar_icon.action != wuss_MOUSE_UP)
    return result_OK;
  if (event->data.iconbar_icon.button & wuss_BUTTON_MENU)
    return tasks_open_menu();
  return result_OK; /* SELECT and ADJUST: nothing yet */
}

result_t taskmanager_create(wuss_t *wuss)
{
  result_t                 rc;
  wuss_task_t             *wtask;
  wuss_task_desc_t         desc;
  wuss_iconbar_icon_spec_t spec;
  wuss_iconbar_icon_t     *icon;

  desc.handle    = taskmanager_handle;
  desc.task_data = NULL;
  desc.name      = "taskmanager";
  rc = wuss_task_create(wuss, &desc, &wtask);
  if (rc != result_OK)
    return rc;

  spec.text  = NULL;
  spec.side  = wuss_ICONBAR_RIGHT;
  spec.image = wuss_task_iconpool_find(wtask, "file_app");
  if (spec.image == NULL)
    logf_warning("taskmanager: icon 'file_app' unavailable");
  rc = wuss_iconbar_icon_create(wuss, wtask, &spec, &icon);
  if (rc != result_OK)
    wuss_task_destroy(wtask);

  return rc;
}

#endif /* WUSS_ICONBAR */
