/* wuss/window/get-task.c -- fetch the task owning a window */

#include <stddef.h>

#include "wuss/window.h"

#include "../impl.h"

wuss_task_t *wuss_window_get_task(const wuss_window_t *window)
{
  return window->task;
}
