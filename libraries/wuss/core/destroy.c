/* wuss/destroy.c -- wuss - minimal window manager */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "impl.h"

void wuss_destroy(wuss_t *doomed)
{
  list_t      *e;
  int          s;
  wuss_task_t *iconbar_task;

  if (doomed == NULL)
    return;

#ifdef WUSS_ICONBAR
  iconbar_task = doomed->iconbar.task;
#else
  iconbar_task = NULL;
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
   * created lazily on the first wuss_menu_open, so it can sit anywhere).
   *
   * Mark every client task as being reaped before any QUIT goes out, so a
   * handler that closes the last autoclose window of any task -- or calls
   * wuss_task_destroy on any task, itself included -- doesn't fire QUIT and
   * free a node from under the sweep. Marking only the current task would
   * leave the saved `next` node open to that. Same guard wuss_task_destroy
   * sets for its own internal QUIT. */
  for (e = doomed->tasks.next; e != NULL; e = e->next)
    if (!wuss__menu_is_task(doomed, wuss__task_from_link(e)) &&
        wuss__task_from_link(e) != iconbar_task)
      wuss__task_from_link(e)->flags |= wuss_TASK__REAPING;

  e = doomed->tasks.next;
  while (e != NULL)
  {
    list_t      *next;

    next = e->next;
    if (!wuss__menu_is_task(doomed, wuss__task_from_link(e)) &&
        wuss__task_from_link(e) != iconbar_task)
    {
      wuss_task_t *task = wuss__task_from_link(e);

      /* If this task owns the live menu chain, abandon it before QUIT, same
       * as wuss_task_destroy does for a standalone task teardown: this also
       * delivers wuss_EVENT_MENU_CLOSED to `task`, nulling any
       * wuss_menu_handle_t it holds, so a QUIT handler's own
       * wuss_menu_close on that handle doesn't walk an already-freed chain
       * -- the leftover-chain close below then just finds nothing to do. */
      wuss__menu_task_closing(doomed, task);

      (void) wuss__deliver_kind(task, NULL, wuss_EVENT_QUIT);

      /* Close the task's windows before freeing it, as wuss_task_destroy
       * does: left for the z_order sweep, they would point at the freed
       * task, and a later task's teardown could still deliver to them -- an
       * abandoned borrowed menu level handing the focus back to one, say.
       * Closing also clears any menu level's saved focus on them.
       * REAPING (set above) keeps autoclose from freeing the task here. */
      while (task->windows.next != NULL)
        wuss_window_close(wuss__window_from_task_link(task->windows.next));

      wuss__free(doomed, e);
    }
    e = next;
  }

  /* Close any chain a QUIT handler left open, then free the menu task: it
   * outlived every client QUIT above so such a handler could close its own
   * chain against live menu windows. */
  wuss__menu_teardown(doomed);

#ifdef WUSS_ICONBAR
  /* The bar's window is freed by the z_order sweep below like any window;
   * only its owning task needs freeing here, same slot in the sequence as
   * menu_task -- no window/menu-chain machinery of its own to abandon
   * first. */
  if (iconbar_task != NULL)
  {
    (void) wuss__deliver_kind(iconbar_task, NULL, wuss_EVENT_QUIT);
    wuss__free(doomed, &iconbar_task->link);
    doomed->iconbar.task = NULL;
  }
#endif

  for (s = 0; s < WUSS_STACK_COUNT; s++)
  {
    e = doomed->z_order[s].next;
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
  }

#ifdef WUSS_ICONS
  wuss__iconpool_clear(doomed, &doomed->icon);
#endif

#ifdef WUSS_ICONBAR
  wuss__iconbar_free(doomed);
#endif

  bmfontcache_destroy(doomed->font_cache); /* after the task sweep above, so
                                            * every acquired font has already
                                            * been released */
  wuss__free(doomed, doomed->msg.queue);
  wuss__menu_session_free(doomed);
  wuss__free(doomed, doomed->palette);
  wuss__free(doomed, doomed->clipboard);
  wuss__free(doomed, doomed); /* reads doomed->alloc.free before freeing */
}
