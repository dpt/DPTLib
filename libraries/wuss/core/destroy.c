/* wuss/destroy.c -- wuss - minimal window manager */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "impl.h"

void wuss_destroy(wuss_t *doomed)
{
  list_t      *e;
  wuss_task_t *menu_task;

  if (doomed == NULL)
    return;

#ifdef WUSS_MENUS
  menu_task = doomed->menu_task;
#else
  menu_task = NULL;
#endif

  /* QUIT then free any tasks the caller left registered, so a task's
   * client-owned task_data (freed only from its QUIT handler, per the
   * task_data-ownership contract) is not leaked. Done before the menu-chain
   * and window teardown below so a QUIT handler can still close its own
   * windows and its own menu chain (via a wuss_menu_handle_t it still holds)
   * against live objects -- the leftover-chain close and the z_order sweep
   * then just find less to do.
   *
   * The internal wuss__menu_task is skipped here and torn down last: a client
   * task's QUIT handler may wuss_menu_close a chain it still holds, which
   * closes menu windows off wuss__menu_task's window list -- so that task
   * must outlive every client QUIT, regardless of registration order (it is
   * created lazily on the first wuss_menu_open, so it can sit anywhere). */
  e = doomed->tasks.next;
  while (e != NULL)
  {
    list_t      *next;
    wuss_event_t event;

    next = e->next;
    if (wuss__task_from_link(e) != menu_task)
    {
      wuss_task_t *task = wuss__task_from_link(e);

      /* Mark the teardown before QUIT so a handler that closes its last
       * autoclose window -- or calls wuss_window_close / wuss_task_destroy
       * on itself -- doesn't also fire QUIT and free this node from under
       * the sweep, which would then wuss__free it a second time. Same guard
       * wuss_task_destroy sets for its own internal QUIT. */
      task->flags |= wuss_TASK__REAPING;

#ifdef WUSS_MENUS
      /* If this task owns the live menu chain, abandon it before QUIT, same
       * as wuss_task_destroy does for a standalone task teardown: this also
       * delivers wuss_EVENT_MENU_CLOSED to `task`, nulling any
       * wuss_menu_handle_t it holds, so a QUIT handler's own
       * wuss_menu_close on that handle doesn't walk an already-freed chain
       * -- the leftover-chain close below then just finds nothing to do. */
      if (doomed->menu_chain != NULL && doomed->menu_chain->owner == task)
        wuss__menu_abandon(doomed);
#endif

      event.kind = wuss_EVENT_QUIT;
      (void) wuss__deliver(task, NULL, &event);
      wuss__free(doomed, e);
    }
    e = next;
  }

#ifdef WUSS_MENUS
  /* Drop any menu chain a QUIT handler left open: its windows are freed by
   * the z_order sweep below, but the chain nodes and their icon-handle
   * arrays are not. NULL (the common case now) is a no-op. Still runs
   * against a live wuss__menu_task.
   *
   * ponytail: a bare wuss_menu_close, not wuss__menu_abandon -- every client
   * task is already freed by the sweep above, so the chain's owner (and any
   * flashing pick's owner) is dangling and must not be delivered to. No path
   * leaves a mid-flash pick standing at wuss_destroy today; track the flash
   * owner per task if one ever does. */
  wuss_menu_close(doomed->menu_chain);

  /* Now the menu task has nothing left to own -- QUIT and free it. */
  if (menu_task != NULL)
  {
    wuss_event_t event;

    event.kind = wuss_EVENT_QUIT;
    (void) wuss__deliver(menu_task, NULL, &event);
    wuss__free(doomed, &menu_task->link);
    doomed->menu_task = NULL;
  }
#endif

  e = doomed->z_order.next;
  while (e != NULL)
  {
    list_t *next;

    next = e->next;
#ifdef WUSS_ICONS
    wuss__icons_free(wuss__window_from_link(e));
#endif
    wuss__free(doomed, e);
    e = next;
  }

#ifdef WUSS_ICONS
  wuss__icons_registry_free(doomed);
#endif

  packer_destroy(doomed->layout);
  wuss__free(doomed, doomed->palette);
  wuss__free(doomed, doomed); /* reads doomed->alloc.free before freeing */
}
