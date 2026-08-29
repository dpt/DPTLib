/* close.c -- wuss - minimal window manager */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../impl.h"

void wuss_window_close(wuss_window_t *doomed)
{
  wuss_t *wuss;

  if (doomed == NULL)
    return;

  wuss = doomed->wuss;
  if (wuss->furniture.dragging == doomed)
    wuss->furniture.dragging = NULL;

  wuss__invalidate_clipped(doomed, &doomed->visible);

  list_remove(&wuss->z_order, &doomed->link);

  wuss__icons_free(doomed);

  free(doomed);
}
