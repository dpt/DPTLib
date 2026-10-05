/* wuss/task/set-parent.c -- parent subsequently created tasks to a task */

#include "wuss/task.h"

#include "../impl.h"

void wuss_set_task_parent(wuss_t *wuss, wuss_task_t *parent)
{
  wuss->task_parent = parent;
}
