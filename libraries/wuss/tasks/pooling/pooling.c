/* wuss/tasks/pooling/pooling.c -- system sprite pool viewer task */

#ifdef WUSS_APP

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "wuss/icon.h"

#include "pooling.h"
#include "../common/taskapp.h"

#define POOLING_LABEL_CHARS 16 /* label column width */
#define POOLING_COLUMNS      6 /* window opens this many cells wide */
#define POOLING_ROWS         6 /* ... and at most this many tall */

/* wuss_gridview_item_fn_t: opaque is the pooling_task_t */
static void pooling_item(int index, void *opaque, wuss_gridview_item_t *out)
{
  pooling_task_t *task;

  task = opaque;

  out->glyph = wuss_iconpool_bitmap(task->wuss, index);
  out->label = wuss_iconpool_name(task->wuss, index);
}

result_t pooling_create(wuss_t *wuss, pooling_task_t **out)
{
  result_t         rc;
  pooling_task_t  *task;
  wuss_task_t     *delegate;
  wuss_task_desc_t delegate_desc;

  task = calloc(1, sizeof(*task));
  if (task == NULL)
    return result_OOM;

  task->wuss = wuss;

  delegate_desc.handle    = pooling_handle;
  delegate_desc.task_data = task;
  delegate_desc.name      = "pooling";
  rc = wuss_task_create(wuss, &delegate_desc, &delegate);
  if (rc != result_OK)
  {
    free(task); /* nothing registered yet; the spawner will not free it */
    return rc;
  }
  wuss_task_set_autoclose(delegate, 1);
  task->delegate = delegate;

  rc = wuss_gridview_window_create(&task->gv, delegate, NULL, "Sprite pool",
                                   wuss_iconpool_count(wuss), pooling_item,
                                   POOLING_LABEL_CHARS, NULL, task, 0,
                                   &task->window);
  if (rc != result_OK)
  {
    wuss_task_destroy(delegate); /* its QUIT frees the task block */
    return rc;
  }

  wuss_gridview_fit_window(task->gv, POOLING_COLUMNS, POOLING_ROWS);

  if (out)
    *out = task;

  return result_OK;
}

result_t pooling_app_create(wuss_t *wuss)
{
  static const taskapp_desc_t desc =
  {
    "Pooling",
    "System sprite pool viewer",
    (taskapp_create_fn_t) pooling_create
  };

  return taskapp_install(wuss, &desc);
}

void pooling_destroy(pooling_task_t *task)
{
  wuss_gridview_destroy(task->gv);
  free(task);
}

result_t pooling_handle(wuss_window_t      *window,
                        const wuss_event_t *event,
                        void               *task_data)
{
  pooling_task_t *pt;

  pt = task_data;

  switch (event->kind)
  {
  case wuss_EVENT_CLOSE:
    if (window == pt->window)
      pt->window = NULL;
    return result_OK;

  case wuss_EVENT_QUIT:
    pooling_destroy(pt);
    return result_OK;

  default:
    if (pt->gv != NULL)
      (void) wuss_gridview_handle_event(pt->gv, event);
    return result_OK;
  }
}

#endif /* WUSS_APP */
