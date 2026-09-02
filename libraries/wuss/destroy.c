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
    wuss__icons_free((wuss_window_t *) e);
#endif
    wuss__free(doomed, e);
    e = next;
  }

  packer_destroy(doomed->layout);
  wuss__free(doomed, doomed->palette);
  wuss__free(doomed, doomed); /* reads doomed->alloc.free before freeing */
}
