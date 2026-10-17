/* wuss/task/raise-children.c -- bring a task's child tasks' windows to front */

#include <stddef.h>

#include "wuss/task.h"
#include "wuss/window.h"

#include "../impl.h"

int wuss_task_raise_children(wuss_t *wuss, wuss_task_t *parent)
{
  list_t      *t;
  list_t      *w;
  wuss_task_t *child;
  int          found;

  found = 0;
  for (t = wuss->tasks.next; t != NULL; t = t->next)
  {
    child = wuss__task_from_link(t);
    if (child->parent != parent)
      continue;

    found = 1;
    for (w = child->windows.next; w != NULL; w = w->next)
      wuss_window_restack(wuss__window_from_task_link(w), wuss_ZORDER_FRONT);
  }

  return found;
}
