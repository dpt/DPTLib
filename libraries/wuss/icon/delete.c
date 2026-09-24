/* wuss/icon/delete.c -- destroy a work-area icon */

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../core/impl.h"

void wuss_icon_delete(wuss_window_t *window, wuss_icon_t *icon)
{
  int i;

  if (icon == NULL)
    return;

  if (window->wuss->pressed_icon == icon)
  {
    window->wuss->pressed_icon   = NULL;
    window->wuss->pressed_window = NULL;
  }
  if (window->wuss->hover_icon == icon)
  {
    window->wuss->hover_icon   = NULL;
    window->wuss->hover_window = NULL;
  }

  if (window->wuss->caret_icon == icon)
  {
    window->wuss->caret_icon   = NULL;
    window->wuss->caret_window = NULL;
  }

  wuss__icon_invalidate(window, icon);

  for (i = 0; i < window->nicons; i++)
  {
    if (window->icons[i] == icon)
    {
      /* Shift the rest down rather than swapping the last one in: array
       * order is draw and hit-test order, so it must be kept. */
      window->nicons--;
      memmove(&window->icons[i], &window->icons[i + 1],
              (size_t) (window->nicons - i) * sizeof(window->icons[0]));
      break;
    }
  }

  wuss__free(window->wuss, (char *) icon->spec.text);
  wuss__free(window->wuss, icon);
}
