/* wuss/task/close-windows.c -- veto-checked close of every window of a task */

#include <stddef.h>

#include "wuss/task.h"
#include "wuss/window.h"

#include "../impl.h"

result_t wuss_task_close_windows(wuss_t *wuss, wuss_task_t *task)
{
  result_t       rc;
  list_t        *t;
  wuss_window_t *window;

  for (;;)
  {
    /* the last close may destroy an autoclose task, so check it is still
     * registered before touching it */
    for (t = wuss->tasks.next; t != NULL; t = t->next)
      if (wuss__task_from_link(t) == task)
        break;

    if (t == NULL || task->windows.next == NULL)
      return result_OK;

    window = wuss__window_from_task_link(task->windows.next);
    rc     = wuss_window_try_close(window);
    if (rc != result_OK)
      return rc;
  }
}
