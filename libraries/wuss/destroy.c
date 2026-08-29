/* destroy.c -- wuss - minimal window manager */

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

  e = doomed->z_order.next;
  while (e != NULL)
  {
    list_t *next;

    next = e->next;
    wuss__icons_free((wuss_window_t *) e);
    free(e);
    e = next;
  }

  free(doomed->palette);
  free(doomed);
}
