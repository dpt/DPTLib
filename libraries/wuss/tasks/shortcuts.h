/* wuss/tasks/shortcuts.h -- system keyboard shortcut list task */

#ifndef TASKS_SHORTCUTS_H
#define TASKS_SHORTCUTS_H

#ifdef WUSS_APP

#include "wuss/gadget/gridview.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* shows every system-wide keyboard shortcut as a two-column grid: the keys,
 * then what they do */
typedef struct shortcuts_task
{
  wuss_t          *wuss;
  wuss_task_t     *delegate;
  wuss_window_t   *window;
  wuss_gridview_t *gv;
}
shortcuts_task_t;

wuss_window_fn_t shortcuts_handle;

/* create the shortcut list window against the given wuss instance; if out is
 * non-NULL, the task block is also returned through it */
result_t shortcuts_create(wuss_t *wuss, shortcuts_task_t **out);

/* install the icon bar app that opens shortcut lists on demand */
result_t shortcuts_app_create(wuss_t *wuss);

/* free a task block allocated by shortcuts_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void shortcuts_destroy(shortcuts_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_SHORTCUTS_H */
