/* start.c -- wuss - minimal window manager */

#include <stddef.h>

#include "wuss/task.h"

wuss_task_t wuss_task_start(wuss_event_fn_t *handle,
                            void            *task_data)
{
  wuss_task_t task;

  task.handle    = handle;
  task.task_data = task_data;

  return task;
}
