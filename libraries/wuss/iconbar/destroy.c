/* wuss/iconbar/destroy.c -- remove an icon from the icon bar */

#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../core/impl.h"

#ifdef WUSS_ICONBAR

void wuss_iconbar_icon_destroy(wuss_t *wuss, wuss_iconbar_icon_t *icon)
{
  int   i;
  box_t bar;

  if (icon == NULL)
    return;

  for (i = 0; i < wuss->niconbar_icons; i++)
  {
    if (wuss->iconbar_icons[i] == icon)
    {
      /* Shift the rest down: array order is left-to-right slot order, and
       * every icon to the right of the removed one moves one slot left. */
      wuss->niconbar_icons--;
      memmove(&wuss->iconbar_icons[i], &wuss->iconbar_icons[i + 1],
              (size_t) (wuss->niconbar_icons - i) *
              sizeof(wuss->iconbar_icons[0]));
      break;
    }
  }

  wuss__iconbar_box(wuss, &bar);
  wuss__invalidate_clipped(wuss->iconbar_window, &bar);

  wuss__free(wuss, (char *) icon->spec.text);
  wuss__free(wuss, icon);
}

#endif /* WUSS_ICONBAR */
