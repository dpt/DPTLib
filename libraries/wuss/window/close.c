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

#ifdef WUSS_ICONS
  wuss__icons_free(doomed);
#endif

  free(doomed);
}
