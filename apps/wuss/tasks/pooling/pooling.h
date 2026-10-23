/* wuss/tasks/pooling/pooling.h -- system sprite pool viewer task */

#ifndef TASKS_POOLING_H
#define TASKS_POOLING_H

#ifdef WUSS_APP

#include "wuss/gadget/gridview.h"
#include "wuss/task.h"
#include "wuss/window.h"

/* shows every entry of the shared icon pool (the system sprites) as a grid
 * of bitmaps labelled with their names */
typedef struct pooling_task
{
  wuss_t          *wuss;
  wuss_task_t     *delegate;
  wuss_window_t   *window;
  wuss_gridview_t *gv;
}
pooling_task_t;

wuss_window_fn_t pooling_handle;

/* create the pool viewer window against the given wuss instance; if out is
 * non-NULL, the task block is also returned through it */
result_t pooling_create(wuss_t *wuss, pooling_task_t **out);

/* install the icon bar app that opens pool viewers on demand */
result_t pooling_app_create(wuss_t *wuss);

/* free a task block allocated by pooling_create; normally called by the
 * window's wuss_EVENT_QUIT handler, not by callers directly */
void pooling_destroy(pooling_task_t *task);

#endif /* WUSS_APP */

#endif /* TASKS_POOLING_H */
