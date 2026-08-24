/* window-destroy.c -- wuss - minimal window manager */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "impl.h"

void wuss_window_destroy(wuss_window_t *doomed)
{
  wuss_t *wuss;

  if (doomed == NULL)
    return;

  wuss = doomed->wuss;
  if (wuss->dragging == doomed)
    wuss->dragging = NULL;

  list_remove(&wuss->z_order, &doomed->link);

  wuss_invalidate(wuss, &doomed->visible);

  free(doomed);
}
