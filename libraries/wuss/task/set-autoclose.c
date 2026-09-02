/* wuss/task/set-autoclose.c -- opt a task into self-destruct on last close */

#include <stddef.h>

#include "wuss/task.h"

#include "../impl.h"

void wuss_task_set_autoclose(wuss_task_t *task, int on)
{
  if (task == NULL)
    return;

  if (on)
    task->flags |= wuss_TASK__AUTOCLOSE;
  else
    task->flags &= ~wuss_TASK__AUTOCLOSE;
}
