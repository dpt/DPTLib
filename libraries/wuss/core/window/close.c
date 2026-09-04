/* wuss/window/close.c -- wuss - minimal window manager */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../impl.h"

void wuss_window_close(wuss_window_t *doomed)
{
  wuss_task_t  *task;
  wuss_event_t  event;
  wuss_t       *wuss;

  if (doomed == NULL)
    return;

  wuss = doomed->wuss;
  task = doomed->task;
#ifdef WUSS_FURNITURE
  if (wuss->furniture.dragging == doomed)
    wuss->furniture.dragging = NULL;
#endif
#ifdef WUSS_ICONS
  if (wuss->pressed_icon != NULL && wuss->pressed_icon->window == doomed)
    wuss->pressed_icon = NULL;
  if (wuss->hover_icon != NULL && wuss->hover_icon->window == doomed)
    wuss->hover_icon = NULL;
#endif

  wuss__release_packed(doomed);

  wuss__invalidate_clipped(doomed, &doomed->visible);

  list_remove(&wuss->z_order, &doomed->link);
  list_remove(&task->windows, &doomed->task_link);

#ifdef WUSS_ICONS
  wuss__icons_free(doomed);
#endif

  wuss__free(wuss, doomed);

  /* An autoclose task self-destructs once it loses its last window: QUIT
   * (so the handler can free task_data), then unlink and free the node.
   * Suppressed while wuss_task_destroy is already tearing the task down. */
  if ((task->flags & wuss_TASK__AUTOCLOSE) &&
      !(task->flags & wuss_TASK__REAPING)  &&
      task->windows.next == NULL)
  {
    event.kind = wuss_EVENT_QUIT;
    (void) wuss__deliver(task, NULL, &event);

    list_remove(&wuss->tasks, &task->link);
    wuss__free(wuss, task);
  }
}
