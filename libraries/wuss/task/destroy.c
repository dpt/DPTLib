/* wuss/task/destroy.c -- unregister and free a task */

#include <stddef.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "wuss/task.h"

#include "../impl.h"

void wuss_task_destroy(wuss_task_t *doomed)
{
  wuss_event_t event;
  wuss_t      *wuss;
  list_t      *e;

  if (doomed == NULL)
    return;

  wuss = doomed->wuss;

  /* One QUIT while every window is still alive. */
  event.kind = wuss_EVENT_QUIT;
  (void) wuss__deliver(doomed, NULL, &event);

  /* Force-close every window the task owns, in task-list order. No
   * PRE_CLOSE/CLOSE -- wuss_window_close is the unvetoable teardown. Each
   * close unlinks the window's task_link, so re-read the head each time. */
  while ((e = doomed->windows.next) != NULL)
    wuss_window_close(wuss__window_from_task_link(e));

  list_remove(&wuss->tasks, &doomed->link);
  wuss__free(wuss, doomed);
}
