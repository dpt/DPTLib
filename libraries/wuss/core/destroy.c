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

#ifdef WUSS_MENUS
  /* Drop any open menu chain first: its windows are freed by the z_order
   * sweep below, but the chain nodes and their icon-handle arrays are not. */
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

  /* Windows are gone; QUIT then free any tasks the caller left registered,
   * so a task's client-owned task_data (freed only from its QUIT handler,
   * per the task_data-ownership contract) is not leaked. */
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

  packer_destroy(doomed->layout);
  wuss__free(doomed, doomed->palette);
  wuss__free(doomed, doomed); /* reads doomed->alloc.free before freeing */
}
