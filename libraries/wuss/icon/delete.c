/* wuss/icon/delete.c -- destroy a work-area icon */

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../core/types.h"
#include "../core/alloc.h"
#include "../core/dispatch.h"

void wuss_icon_delete(wuss_window_t *window, wuss_icon_t *icon)
{
  int i;

  if (icon == NULL)
    return;

  if (window->wuss->pressed.icon == icon)
  {
    window->wuss->pressed.icon   = NULL;
    window->wuss->pressed.window = NULL;
  }
  if (window->wuss->hover.icon == icon)
  {
    window->wuss->hover.icon   = NULL;
    window->wuss->hover.window = NULL;
  }

  if (window->wuss->caret.icon == icon)
  {
    window->wuss->caret.icon   = NULL;
    window->wuss->caret.window = NULL;
  }

  wuss__message_purge_icon(window->wuss, icon);

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
