/* wuss/icon/delete.c -- destroy a work-area icon */

#include <stdlib.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../impl.h"

void wuss_icon_delete(wuss_icon_t *icon)
{
  wuss_window_t *window;
  int            i;

  if (icon == NULL)
    return;

  window = icon->window;

  if (window->wuss->pressed_icon == icon)
    window->wuss->pressed_icon = NULL;
  if (window->wuss->hover_icon == icon)
    window->wuss->hover_icon = NULL;

  wuss__icon_invalidate(icon);

  for (i = 0; i < window->nicons; i++)
  {
    if (window->icons[i] == icon)
    {
      window->icons[i] = window->icons[--window->nicons];
      break;
    }
  }

  wuss__free(window->wuss, icon->text);
  wuss__free(window->wuss, icon);
}
