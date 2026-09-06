/* wuss/destroy.c -- wuss - minimal window manager */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "impl.h"

void wuss_destroy(wuss_t *doomed)
{
  list_t *e;

  if (doomed == NULL)
    return;

  /* QUIT then free any tasks the caller left registered, so a task's
   * client-owned task_data (freed only from its QUIT handler, per the
   * task_data-ownership contract) is not leaked. Done before the menu-chain
   * and window teardown below so a QUIT handler can still close its own
   * windows and its own menu chain (via a wuss_menu_handle_t it still holds)
   * against live objects -- the leftover-chain close and the z_order sweep
   * then just find less to do.
   *
   * ponytail: a task whose QUIT handler leaves an open chain standing, and
   * whose flashing pick then gets delivered by the wuss_menu_close below to
   * an already-freed earlier task, would fault. No task does that today; fix
   * by tracking chain ownership per task if one ever needs to. */
  e = doomed->tasks.next;
  while (e != NULL)
  {
    list_t      *next;
    wuss_event_t event;

    next = e->next;
    event.kind = wuss_EVENT_QUIT;
    (void) wuss__deliver(wuss__task_from_link(e), NULL, &event);
    wuss__free(doomed, e);
    e = next;
  }

#ifdef WUSS_MENUS
  /* Drop any menu chain a QUIT handler left open: its windows are freed by
   * the z_order sweep below, but the chain nodes and their icon-handle
   * arrays are not. NULL (the common case now) is a no-op. */
  wuss_menu_close(doomed->menu_chain);
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

  packer_destroy(doomed->layout);
  wuss__free(doomed, doomed->palette);
  wuss__free(doomed, doomed); /* reads doomed->alloc.free before freeing */
}
