/* task-start.c -- wuss - minimal window manager */

#include <stddef.h>

#include "wuss/window.h"

wuss_task_t wuss_task_start(wuss_event_fn_t *handle,
                            void            *task_data,
                            wuss_colour_t    bg)
{
  wuss_task_t task;

  task.handle    = handle;
  task.task_data = task_data;
  task.bg        = bg;

  return task;
}
