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
  wuss_task_t *child;

  if (doomed == NULL)
    return;

  /* Already being reaped -- by wuss_destroy's sweep, or a re-entrant call
   * from this task's own QUIT handler. The outer teardown owns the unlink
   * and free; doing it here too would double-free the node. */
  if (doomed->flags & wuss_TASK__REAPING)
    return;

  wuss = doomed->wuss;

  /* Mark the teardown so wuss_window_close's autoclose path doesn't also
   * fire QUIT and free the node from under us. */
  doomed->flags |= wuss_TASK__REAPING;

#ifdef WUSS_MENUS
  /* If this task opened the live menu chain, tear it down now: leaving it
   * open would leave its owner (and any pending pick flash's owner) pointing
   * at `doomed` after it is freed below. wuss__menu_abandon also delivers
   * wuss_EVENT_MENU_CLOSED to `doomed`, so a wuss_menu_handle_t it still
   * holds is nulled before the QUIT below -- otherwise a QUIT handler that
   * calls wuss_menu_close on that stale handle double-frees the chain. */
  if (wuss->menu.chain != NULL && wuss->menu.chain->owner == doomed)
    wuss__menu_abandon(wuss);
#endif

  /* Children first, so each sees its own QUIT before the parent's. Each
   * destroy unlinks the child (and any autoclose sibling it brings down), so
   * restart the scan from the head every time. */
  do
  {
    child = NULL;
    for (e = wuss->tasks.next; e != NULL; e = e->next)
      if (wuss__task_from_link(e)->parent == doomed &&
          !(wuss__task_from_link(e)->flags & wuss_TASK__REAPING))
      {
        child = wuss__task_from_link(e);
        break;
      }
    wuss_task_destroy(child);
  }
  while (child != NULL);

  /* One QUIT while every window is still alive. */
  event.kind = wuss_EVENT_QUIT;
  (void) wuss__deliver(doomed, NULL, &event);

  /* Drop anything this task sent before the close loop below bounces
   * messages addressed to its windows, so none bounce back to it. */
  wuss__message_purge_task(wuss, doomed);

  /* Force-close every window the task owns, in task-list order. No
   * PRE_CLOSE/CLOSE -- wuss_window_close is the unvetoable teardown. Each
   * close unlinks the window's task_link, so re-read the head each time. */
  while ((e = doomed->windows.next) != NULL)
    wuss_window_close(wuss__window_from_task_link(e));

#ifdef WUSS_ICONBAR
  wuss__iconbar_task_destroyed(wuss, doomed);
#endif

#ifdef WUSS_ICONS
  /* After the windows: their icons point into this pool. */
  wuss__iconpool_clear(wuss, &doomed->iconpool);
#endif

  list_remove(&wuss->tasks, &doomed->link);
  wuss__free(wuss, doomed);
}
